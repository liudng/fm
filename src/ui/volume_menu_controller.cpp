#include "volume_menu_controller.h"

#include "../core/volume_manager.h"

#include <QAction>
#include <QFutureWatcher>
#include <QMenu>
#include <QMouseEvent>
#include <QtConcurrent>

namespace fm {

VolumeMenuController::VolumeMenuController(QMenu *fileMenu, QObject *parent)
    : QObject(parent), fileMenu_(fileMenu)
{}

void VolumeMenuController::setup()
{
    // 卷项在 aboutToShow 时动态插入到 volSeparator_ 之前；
    // 外部设备项动态插入到 volSeparator_ 与 extSeparator_ 之间
    volSeparator_ = fileMenu_->addSeparator();
    extSeparator_ = fileMenu_->addSeparator();
    connect(fileMenu_, &QMenu::aboutToShow, this, &VolumeMenuController::refresh);
    // 安装事件过滤器，支持右键挂载/卸载/弹出
    fileMenu_->installEventFilter(this);
}

void VolumeMenuController::refresh()
{
    if (!fileMenu_ || !volSeparator_ || !extSeparator_) return;

    // 移除旧卷项
    for (QAction *a : volActions_) {
        fileMenu_->removeAction(a);
        a->deleteLater();
    }
    volActions_.clear();

    // 移除旧外部设备项（含可能存在的"加载中"占位）
    for (QAction *a : extActions_) {
        fileMenu_->removeAction(a);
        a->deleteLater();
    }
    extActions_.clear();

    // === 卷段（已挂载卷，QStorageInfo 本地查询，快）===
    const QList<VolumeInfo> volumes = VolumeManager::instance()->listVolumes();
    if (volumes.isEmpty()) {
        auto *placeholder = fileMenu_->addAction(tr("(No volumes)"));
        placeholder->setEnabled(false);
        fileMenu_->insertAction(volSeparator_, placeholder);
        volActions_.append(placeholder);
    } else {
        for (const VolumeInfo &v : volumes) {
            QString text = v.label.isEmpty() ? v.mountPoint : v.label;
            if (!v.mountPoint.isEmpty() && text != v.mountPoint) {
                text += QStringLiteral("  (%1)").arg(v.mountPoint);
            }
            auto *act =
                new QAction(QIcon::fromTheme(QStringLiteral("drive-harddisk")), text, fileMenu_);
            // data 存储挂载点（左键导航用）；deviceFile/isMounted 供右键用
            act->setData(v.mountPoint);
            act->setProperty("deviceFile", v.deviceFile);
            act->setProperty("isMounted", true); // 卷项均为已挂载
            connect(act, &QAction::triggered, this,
                    [this, mp = v.mountPoint]() { emit navigateRequested(mp); });
            fileMenu_->insertAction(volSeparator_, act);
            volActions_.append(act);
        }
    }

    // === 外部设备段（含未挂载，UDisks2 D-Bus 查询，慢）===
    // 先显示"加载中"占位，避免阻塞菜单弹出；后台异步枚举完成后替换
    auto *loadingAct = new QAction(tr("Loading devices..."), fileMenu_);
    loadingAct->setEnabled(false);
    fileMenu_->insertAction(extSeparator_, loadingAct);
    extActions_.append(loadingAct);

    // 异步枚举外部设备（D-Bus 调用放到工作线程，避免阻塞 UI 线程）
    // 多次 aboutToShow 并发时，fillExternalDevices 每次先清空再填充，最终一致
    auto *watcher = new QFutureWatcher<QList<VolumeInfo>>(this);
    connect(watcher, &QFutureWatcher<QList<VolumeInfo>>::finished, this, [this, watcher]() {
        fillExternalDevices(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(
        QtConcurrent::run([]() { return VolumeManager::instance()->listExternalDevices(); }));
}

void VolumeMenuController::fillExternalDevices(const QList<VolumeInfo> &devices)
{
    if (!fileMenu_ || !extSeparator_) return;

    // 清空当前外部设备项（移除"加载中"占位或上一次异步结果）
    for (QAction *a : extActions_) {
        fileMenu_->removeAction(a);
        a->deleteLater();
    }
    extActions_.clear();

    if (devices.isEmpty()) {
        auto *placeholder = fileMenu_->addAction(tr("(No external devices)"));
        placeholder->setEnabled(false);
        fileMenu_->insertAction(extSeparator_, placeholder);
        extActions_.append(placeholder);
    } else {
        for (const VolumeInfo &d : devices) {
            // 设备名称：设备文件名必显示；有卷标/型号时前置
            QString text = d.deviceFile;
            if (!d.label.isEmpty()) {
                text = QStringLiteral("%1 (%2)").arg(d.label, d.deviceFile);
            }
            // 已挂载追加挂载点
            if (d.isMounted && !d.mountPoint.isEmpty()) {
                text += QStringLiteral("  (%1)").arg(d.mountPoint);
            }
            auto *act =
                new QAction(QIcon::fromTheme(QStringLiteral("media-removable")), text, fileMenu_);
            act->setProperty("deviceFile", d.deviceFile);
            act->setProperty("isMounted", d.isMounted);
            act->setProperty("mountPoint", d.mountPoint);
            // 左键仅选中不操作（不连接 triggered）
            fileMenu_->insertAction(extSeparator_, act);
            extActions_.append(act);
        }
    }
}

bool VolumeMenuController::eventFilter(QObject *obj, QEvent *event)
{
    // 文件菜单卷项/外部设备项右键：挂载/卸载/弹出
    if (obj == fileMenu_ && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::RightButton) {
            auto *menu = static_cast<QMenu *>(obj);
            QAction *act = menu->actionAt(me->pos());
            if (act && (volActions_.contains(act) || extActions_.contains(act))) {
                showContextMenu(act, me->globalPosition().toPoint());
                return true; // 事件已处理
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void VolumeMenuController::showContextMenu(QAction *act, const QPoint &globalPos)
{
    const QString deviceFile = act->property("deviceFile").toString();
    if (deviceFile.isEmpty()) return;

    const bool isExternal = extActions_.contains(act);
    const bool isMounted = act->property("isMounted").toBool();
    QMenu ctx(fileMenu_);
    QAction *mountAct = nullptr, *unmountAct = nullptr, *ejectAct = nullptr;
    if (isExternal && !isMounted) {
        // 未挂载外部设备：仅显示挂载
        mountAct = ctx.addAction(tr("Mount"));
        mountAct->setIcon(QIcon::fromTheme(QStringLiteral("media-mount")));
    } else {
        // 卷项（已挂载）或已挂载设备项：卸载/弹出
        unmountAct = ctx.addAction(tr("Safely Unmount"));
        unmountAct->setIcon(QIcon::fromTheme(QStringLiteral("media-eject")));
        ejectAct = ctx.addAction(tr("Eject"));
        ejectAct->setIcon(QIcon::fromTheme(QStringLiteral("media-eject")));
    }
    const QAction *chosen = ctx.exec(globalPos);
    // 确定操作类型：0=mount, 1=unmount, 2=eject, -1=取消
    int op = -1;
    if (chosen == mountAct)
        op = 0;
    else if (chosen == unmountAct)
        op = 1;
    else if (chosen == ejectAct)
        op = 2;
    if (op < 0) return; // 用户取消

    // 异步执行卷操作：Eject/Unmount 可能涉及同步缓冲，耗时数秒，
    // 放到工作线程避免阻塞 UI。完成后回主线程刷新菜单。
    emit statusMessageRequested(tr("Performing volume operation..."), 3000);
    auto *watcher = new QFutureWatcher<bool>(this);
    QString *errMsg = new QString;
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, errMsg]() {
        const bool ok = watcher->result();
        if (!ok && !errMsg->isEmpty()) {
            emit operationFailed(*errMsg);
        }
        // 操作成功后刷新整个文件菜单（挂载/卸载后卷段与设备段都会更新）
        if (ok) refresh();
        delete errMsg;
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([deviceFile, op, errMsg]() -> bool {
        if (op == 0) {
            return VolumeManager::instance()->mount(deviceFile, errMsg);
        } else if (op == 1) {
            return VolumeManager::instance()->unmount(deviceFile, errMsg);
        }
        return VolumeManager::instance()->eject(deviceFile, errMsg);
    }));
}

} // namespace fm
