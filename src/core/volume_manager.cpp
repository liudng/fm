#include "volume_manager.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QDebug>
#include <QFile>
#include <QStorageInfo>

namespace fm {

namespace {

constexpr auto kUDisks2Service = "org.freedesktop.UDisks2";
constexpr auto kUDisks2Path = "/org/freedesktop/UDisks2";
constexpr auto kManagerPath = "/org/freedesktop/UDisks2/Manager";
constexpr auto kManagerIface = "org.freedesktop.UDisks2.Manager";
constexpr auto kBlockIface = "org.freedesktop.UDisks2.Block";
constexpr auto kFsWithIface = "org.freedesktop.UDisks2.Filesystem";
constexpr auto kDriveIface = "org.freedesktop.UDisks2.Drive";

// 从 dbus 属性 map 中读取
QVariant getProperty(const QVariantMap &props, const QString &key) {
    return props.value(key);
}

} // namespace

VolumeManager *VolumeManager::instance() {
    static VolumeManager inst;
    return &inst;
}

VolumeManager::VolumeManager(QObject *parent)
    : QObject(parent) {
}

QStringList VolumeManager::enumerateBlockDevices() {
    QStringList result;
    QDBusInterface iface(kUDisks2Service, kManagerPath, kManagerIface,
                           QDBusConnection::systemBus());
    if (!iface.isValid()) return result;

    QDBusReply<QList<QDBusObjectPath>> reply = iface.call(QStringLiteral("GetBlockDevices"));
    if (!reply.isValid()) return result;

    for (const QDBusObjectPath &p : reply.value()) {
        result.append(p.path());
    }
    return result;
}

VolumeInfo VolumeManager::getBlockDeviceProperties(const QString &blockPath) {
    VolumeInfo info;
    info.devicePath = blockPath;

    // Block 接口
    QDBusInterface blockIface(kUDisks2Service, blockPath, kBlockIface,
                                QDBusConnection::systemBus());
    if (!blockIface.isValid()) return info;

    info.deviceFile = blockIface.property("Device").toString();
    // Device 是 byte array 形式，需要特殊处理
    const QVariant devVar = blockIface.property("Device");
    if (devVar.isValid()) {
        if (devVar.userType() == QMetaType::QByteArray) {
            info.deviceFile = QString::fromUtf8(devVar.toByteArray());
        } else {
            info.deviceFile = devVar.toString();
        }
    }

    // IdUsage=filesystem 且 IdType 才有意义
    const QString idUsage = blockIface.property("IdUsage").toString();
    info.fsType = blockIface.property("IdType").toString();
    if (idUsage != QStringLiteral("filesystem")) {
        return info;
    }

    // 文件系统接口
    QDBusInterface fsIface(kUDisks2Service, blockPath, kFsWithIface,
                              QDBusConnection::systemBus());

    // MountPoints
    const QVariant mpVar = fsIface.property("MountPoints");
    if (mpVar.isValid()) {
        const auto list = qdbus_cast<QList<QByteArray>>(mpVar);
        if (!list.isEmpty()) {
            info.mountPoint = QString::fromUtf8(list.first());
            info.isMounted = !info.mountPoint.isEmpty();
        }
    }

    // 通过 QStorageInfo 获取 label（更可靠）
    if (info.isMounted) {
        const QStorageInfo si(info.mountPoint);
        info.label = si.name();
    }

    // 标记是否外部设备
    const QString drivePath = blockIface.property("Drive").value<QDBusObjectPath>().path();
    if (!drivePath.isEmpty() && drivePath != "/") {
        QDBusInterface driveIface(kUDisks2Service, drivePath, kDriveIface,
                                    QDBusConnection::systemBus());
        if (driveIface.isValid()) {
            const bool removable = driveIface.property("Removable").toBool();
            info.isRemovable = removable;
            info.isExternal = removable;
        }
    }

    // 选择图标
    info.icon = QIcon::fromTheme(info.isRemovable ? QStringLiteral("drive-removable-media")
                                                    : QStringLiteral("drive-harddisk"));

    return info;
}

QList<VolumeInfo> VolumeManager::listVolumes() {
    QList<VolumeInfo> result;
    const QStringList blockPaths = enumerateBlockDevices();
    for (const QString &path : blockPaths) {
        VolumeInfo info = getBlockDeviceProperties(path);
        // 只保留文件系统设备（已挂载或未挂载都可显示）
        if (info.fsType.isEmpty()) continue;
        // 排除根文件系统（已通过 QStorageInfo 列出）
        if (info.mountPoint == QStringLiteral("/")) continue;
        result.append(info);
    }
    return result;
}

bool VolumeManager::mount(const QString &devicePath, QString *errorMsg) {
    QDBusInterface fsIface(kUDisks2Service, devicePath, kFsWithIface,
                            QDBusConnection::systemBus());
    if (!fsIface.isValid()) {
        if (errorMsg) *errorMsg = tr("Invalid device path: %1").arg(devicePath);
        return false;
    }
    // Mount(args) 返回挂载点字符串
    QDBusReply<QString> reply = fsIface.call(QStringLiteral("Mount"), QVariantMap{});
    if (!reply.isValid()) {
        if (errorMsg) *errorMsg = reply.error().message();
        return false;
    }
    return true;
}

bool VolumeManager::unmount(const QString &devicePath, QString *errorMsg) {
    QDBusInterface fsIface(kUDisks2Service, devicePath, kFsWithIface,
                            QDBusConnection::systemBus());
    if (!fsIface.isValid()) {
        if (errorMsg) *errorMsg = tr("Invalid device path: %1").arg(devicePath);
        return false;
    }
    QDBusReply<void> reply = fsIface.call(QStringLiteral("Unmount"), QVariantMap{});
    if (!reply.isValid()) {
        if (errorMsg) *errorMsg = reply.error().message();
        return false;
    }
    emit volumesChanged();
    return true;
}

bool VolumeManager::eject(const QString &devicePath, QString *errorMsg) {
    // Eject 需要通过 Drive 接口
    QDBusInterface blockIface(kUDisks2Service, devicePath, kBlockIface,
                                QDBusConnection::systemBus());
    const QString drivePath = blockIface.property("Drive").value<QDBusObjectPath>().path();
    if (drivePath.isEmpty() || drivePath == "/") {
        if (errorMsg) *errorMsg = tr("No drive for device: %1").arg(devicePath);
        return false;
    }

    QDBusInterface driveIface(kUDisks2Service, drivePath, kDriveIface,
                                QDBusConnection::systemBus());
    QDBusReply<void> reply = driveIface.call(QStringLiteral("Eject"), QVariantMap{});
    if (!reply.isValid()) {
        if (errorMsg) *errorMsg = reply.error().message();
        return false;
    }
    emit volumesChanged();
    return true;
}

} // namespace fm
