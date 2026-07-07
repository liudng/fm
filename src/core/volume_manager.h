#ifndef FM_CORE_VOLUME_MANAGER_H
#define FM_CORE_VOLUME_MANAGER_H

#include <QIcon>
#include <QObject>
#include <QString>
#include <QList>

namespace fm {

// 卷信息
struct VolumeInfo {
    QString devicePath;       // /org/freedesktop/UDisks2/block_devices/sdb1
    QString deviceFile;       // /dev/sdb1
    QString mountPoint;       // /media/user/USB（空表示未挂载）
    QString label;            // 卷标
    QString fsType;           // ext4, vfat, ...
    QString mimeType;
    bool isMounted = false;
    bool isRemovable = false;
    bool isExternal = false;  // 外部设备（U 盘/SD 卡）
    QIcon icon;
};

// 卷管理（单例）
// - 通过 UDisks2 D-Bus 接口枚举卷、挂载、卸载、弹出
// - 同步调用（菜单 aboutToShow 时调用）
class VolumeManager : public QObject {
    Q_OBJECT
public:
    static VolumeManager *instance();

    // 列举所有已挂载/可挂载的块设备文件系统
    QList<VolumeInfo> listVolumes();

    // 挂载设备（返回挂载点；失败返回空字符串）
    QString mount(const QString &devicePath, QString *errorMsg = nullptr);
    // 卸载设备
    bool unmount(const QString &devicePath, QString *errorMsg);
    // 弹出设备（对可移动介质有效）
    bool eject(const QString &devicePath, QString *errorMsg);

signals:
    void volumesChanged();

private:
    VolumeManager(QObject *parent = nullptr);

    // 通过 UDisks2 接口获取块设备路径列表
    QStringList enumerateBlockDevices();
    // 通过设备路径获取属性
    VolumeInfo getBlockDeviceProperties(const QString &blockPath);
};

} // namespace fm

#endif // FM_CORE_VOLUME_MANAGER_H
