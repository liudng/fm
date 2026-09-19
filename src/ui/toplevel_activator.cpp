#include "toplevel_activator.h"

#include <QGuiApplication>
#include <QWindow>

#ifdef FM_HAVE_WAYLAND
#include <QtGui/qguiapplication_platform.h> // QNativeInterface::QWaylandApplication

#include "wlr-foreign-toplevel-protocol.h" // wayland-scanner 构建期生成

#include <wayland-client.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QList>
#include <utility>

namespace fm {
namespace {

struct ToplevelInfo
{
    zwlr_foreign_toplevel_handle_v1 *handle = nullptr;
    QByteArray appId;
    QByteArray title;
    bool minimized = false;
};

struct ActivatorContext
{
    zwlr_foreign_toplevel_manager_v1 *manager = nullptr;
    wl_seat *seat = nullptr;
    QList<ToplevelInfo *> toplevels;
};

void handleTitle(void *data, zwlr_foreign_toplevel_handle_v1 *, const char *title)
{
    static_cast<ToplevelInfo *>(data)->title = title;
}

void handleAppId(void *data, zwlr_foreign_toplevel_handle_v1 *, const char *appId)
{
    static_cast<ToplevelInfo *>(data)->appId = appId;
}

void handleState(void *data, zwlr_foreign_toplevel_handle_v1 *, wl_array *state)
{
    auto *info = static_cast<ToplevelInfo *>(data);
    info->minimized = false;
    const auto *entries = static_cast<const uint32_t *>(state->data);
    for (size_t i = 0; i < state->size / sizeof(uint32_t); ++i) {
        if (entries[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            info->minimized = true;
    }
}

// libwayland 对 NULL listener 槽位会 abort，必须全部填充；
// 清理后的 handle 可能仍有在途事件，槽内不得解引用 data
void handleOutputEnter(void *, zwlr_foreign_toplevel_handle_v1 *, wl_output *) {}
void handleOutputLeave(void *, zwlr_foreign_toplevel_handle_v1 *, wl_output *) {}
void handleDone(void *, zwlr_foreign_toplevel_handle_v1 *) {}
void handleClosed(void *, zwlr_foreign_toplevel_handle_v1 *) {}
void handleParent(void *, zwlr_foreign_toplevel_handle_v1 *, zwlr_foreign_toplevel_handle_v1 *) {}

constexpr zwlr_foreign_toplevel_handle_v1_listener kHandleListener = {
    handleTitle,          // title
    handleAppId,          // app_id
    handleOutputEnter,    // output_enter
    handleOutputLeave,    // output_leave
    handleState,          // state
    handleDone,           // done
    handleClosed,         // closed
    handleParent          // parent (v3)
};

void managerToplevel(void *data, zwlr_foreign_toplevel_manager_v1 *,
                     zwlr_foreign_toplevel_handle_v1 *toplevel)
{
    auto *ctx = static_cast<ActivatorContext *>(data);
    auto *info = new ToplevelInfo;
    info->handle = toplevel;
    ctx->toplevels.append(info);
    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &kHandleListener, info);
}

void managerFinished(void *, zwlr_foreign_toplevel_manager_v1 *) {}

constexpr zwlr_foreign_toplevel_manager_v1_listener kManagerListener = {
    managerToplevel,      // toplevel
    managerFinished       // finished
};

void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                    uint32_t)
{
    auto *ctx = static_cast<ActivatorContext *>(data);
    if (qstrcmp(interface, "zwlr_foreign_toplevel_manager_v1") == 0) {
        ctx->manager = static_cast<zwlr_foreign_toplevel_manager_v1 *>(wl_registry_bind(
                registry, name, &zwlr_foreign_toplevel_manager_v1_interface, 1));
    } else if (qstrcmp(interface, "wl_seat") == 0) {
        ctx->seat = static_cast<wl_seat *>(
                wl_registry_bind(registry, name, &wl_seat_interface, 1));
    }
}

void registryGlobalRemove(void *, wl_registry *, uint32_t) {}

constexpr wl_registry_listener kRegistryListener = {
    registryGlobal,       // global
    registryGlobalRemove  // global_remove
};

} // namespace
} // namespace fm
#endif // FM_HAVE_WAYLAND

namespace fm {

void ToplevelActivator::activateAppToplevel(QWindow *window)
{
#ifdef FM_HAVE_WAYLAND
    if (!window || QGuiApplication::platformName() != QLatin1String("wayland"))
        return;

    // 与 Qt 共用同一 wayland 连接（须在主线程操作）
    auto *waylandApp = qApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    wl_display *display = waylandApp ? waylandApp->display() : nullptr;
    if (!display)
        return;

    ActivatorContext ctx;
    wl_registry *registry = wl_display_get_registry(display);
    if (!registry)
        return;
    wl_registry_add_listener(registry, &kRegistryListener, &ctx);
    wl_display_roundtrip(display); // 全局对象绑定完成

    if (!ctx.manager || !ctx.seat) {
        wl_registry_destroy(registry);
        qDeleteAll(ctx.toplevels);
        return;
    }

    // 枚举全部 toplevel 并收集属性（title/app_id/state 随 handle 事件送达）
    zwlr_foreign_toplevel_manager_v1_add_listener(ctx.manager, &kManagerListener, &ctx);
    wl_display_roundtrip(display);

    // Qt wayland 的 xdg_toplevel app_id：desktopFileName（若有），否则 applicationName
    QByteArray expectedAppId = QGuiApplication::desktopFileName().toUtf8();
    if (expectedAppId.endsWith(".desktop"))
        expectedAppId.chop(8);
    if (expectedAppId.isEmpty())
        expectedAppId = QCoreApplication::applicationName().toUtf8();
    const QByteArray expectedTitle = window->title().toUtf8();

    bool matched = false;
    for (ToplevelInfo *info : std::as_const(ctx.toplevels)) {
        const bool sameApp = !expectedAppId.isEmpty() && info->appId == expectedAppId;
        const bool sameTitle = !expectedTitle.isEmpty() && info->title == expectedTitle;
        if (sameApp || sameTitle) {
            if (info->minimized)
                zwlr_foreign_toplevel_handle_v1_unset_minimized(info->handle);
            zwlr_foreign_toplevel_handle_v1_activate(info->handle, ctx.seat);
            matched = true;
        }
        zwlr_foreign_toplevel_handle_v1_destroy(info->handle);
    }
    zwlr_foreign_toplevel_manager_v1_stop(ctx.manager);
    wl_registry_destroy(registry);
    wl_display_flush(display);
    qDeleteAll(ctx.toplevels);

    if (!matched)
        qWarning() << "ToplevelActivator: no matching toplevel for app"
                   << QString::fromUtf8(expectedAppId);
#else
    Q_UNUSED(window);
#endif
}

} // namespace fm
