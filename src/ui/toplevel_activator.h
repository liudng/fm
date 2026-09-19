#ifndef FM_UI_TOPLEVEL_ACTIVATOR_H
#define FM_UI_TOPLEVEL_ACTIVATOR_H

class QWindow;

namespace fm {

// Wayland 窗口置顶激活器（wlr-foreign-toplevel-management-v1）
// labwc 等 wlroots 合成器支持：按 app_id / 窗口标题匹配本应用的 toplevel，
// 请求合成器激活（等价于任务栏点击）。Wayland 下 raise/activateWindow 由
// 合成器决定，常被忽略，此为唯一可靠的置顶途径。
class ToplevelActivator
{
public:
    // 激活 window 所属应用的 toplevel（须在 GUI 主线程调用）。
    // 仅 wayland 平台有效；协议不可用或无匹配时静默返回，
    // 由调用方的常规 show/raise/activateWindow 兜底。
    static void activateAppToplevel(QWindow *window);
};

} // namespace fm

#endif // FM_UI_TOPLEVEL_ACTIVATOR_H
