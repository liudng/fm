#ifndef FM_UI_VOLUME_MENU_H
#define FM_UI_VOLUME_MENU_H

#include <QObject>
#include <QPointer>

class QMenu;
class QAction;

namespace fm {

// 卷子菜单（嵌入到文件菜单）
// - 刷新时实时枚举卷
// - 左键单击：发出 volumeOpenRequested(mountPoint)
// - 右键（通过子菜单或单独动作）：卸载/弹出
class VolumeMenu : public QObject {
    Q_OBJECT
public:
    explicit VolumeMenu(QMenu *parent);

    // 在菜单 aboutToShow 时调用刷新
    void refresh();

signals:
    void volumeOpenRequested(const QString &mountPoint);
    void volumeUnmountRequested(const QString &devicePath);
    void volumeEjectRequested(const QString &devicePath);

private:
    QPointer<QMenu> menu_;
    QList<QAction*> volumeActions_;
};

} // namespace fm

#endif // FM_UI_VOLUME_MENU_H
