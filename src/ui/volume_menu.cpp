#include "volume_menu.h"

#include "../core/volume_manager.h"

#include <QAction>
#include <QMenu>

namespace fm {

VolumeMenu::VolumeMenu(QMenu *parent)
    : QObject(parent), menu_(parent) {
}

void VolumeMenu::refresh() {
    if (!menu_) return;

    // 先清空旧项
    for (QAction *a : volumeActions_) {
        menu_->removeAction(a);
        a->deleteLater();
    }
    volumeActions_.clear();

    // 枚举卷
    const QList<VolumeInfo> volumes = VolumeManager::instance()->listVolumes();
    if (volumes.isEmpty()) {
        auto *placeholder = new QAction(tr("(No volumes)"), menu_);
        placeholder->setEnabled(false);
        menu_->addAction(placeholder);
        volumeActions_.append(placeholder);
        return;
    }

    for (const VolumeInfo &v : volumes) {
        QString text = v.label.isEmpty() ? v.deviceFile : v.label;
        if (!v.mountPoint.isEmpty()) {
            text += QStringLiteral("  (%1)").arg(v.mountPoint);
        }
        auto *act = new QAction(v.icon, text, menu_);
        act->setData(v.devicePath);
        menu_->addAction(act);

        // 左键：打开挂载点（未挂载则先挂载再打开）
        connect(act, &QAction::triggered, this, [this, v]() {
            if (v.isMounted && !v.mountPoint.isEmpty()) {
                emit volumeOpenRequested(v.mountPoint);
            } else {
                // 未挂载，先挂载
                QString err;
                const QString mp = VolumeManager::instance()->mount(v.devicePath, &err);
                if (mp.isEmpty()) {
                    emit volumeMountFailed(err);
                } else {
                    emit volumeOpenRequested(mp);
                }
            }
        });

        // 子菜单：Open / Unmount / Eject
        auto *ctxMenu = new QMenu(menu_);
        auto *openAction = ctxMenu->addAction(tr("Open"));
        connect(openAction, &QAction::triggered, this, [this, v]() {
            if (v.isMounted && !v.mountPoint.isEmpty()) {
                emit volumeOpenRequested(v.mountPoint);
            } else {
                QString err;
                const QString mp = VolumeManager::instance()->mount(v.devicePath, &err);
                if (mp.isEmpty()) {
                    emit volumeMountFailed(err);
                } else {
                    emit volumeOpenRequested(mp);
                }
            }
        });
        ctxMenu->addSeparator();
        auto *unmountAction = ctxMenu->addAction(tr("Unmount"));
        unmountAction->setEnabled(v.isMounted);
        connect(unmountAction, &QAction::triggered, this, [this, v]() {
            emit volumeUnmountRequested(v.devicePath);
        });
        auto *ejectAction = ctxMenu->addAction(tr("Eject"));
        ejectAction->setEnabled(v.isRemovable);
        connect(ejectAction, &QAction::triggered, this, [this, v]() {
            emit volumeEjectRequested(v.devicePath);
        });
        act->setMenu(ctxMenu);

        volumeActions_.append(act);
    }
}

} // namespace fm
