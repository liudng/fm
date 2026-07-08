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

// 将设备文件路径（/dev/sdb1）转换为 UDisks2 D-Bus 对象路径
// (/org/freedesktop/UDisks2/block_devices/sdb1)
QString toUDisks2ObjectPath(const QString &device) {
    if (device.startsWith(QStringLiteral("/org/freedesktop/UDisks2/"))) {
        return device;  // 已是 D-Bus 路径
    }
    if (device.startsWith(QStringLiteral("/dev/"))) {
        return QStringLiteral("/org/freedesktop/UDisks2/block_devices/")
               + device.mid(5);
    }
    return device;
}

QStringList VolumeManager::enumerateBlockDevices() {
    QStringList result;
    QDBusInterface iface(kUDisks2Service, kManagerPath, kManagerIface,
                           QDBusConnection::systemBus());
    if (!iface.isValid()) return result;

    // GetBlockDevices 签名为 (IN a{sv} options)，必须传一个空 dict 参数
    QDBusReply<QList<QDBusObjectPath>> reply = iface.call(QStringLiteral("GetBlockDevices"), QVariantMap{});
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

    // Device 属性为 ay（字节数组，带尾部 null），用 constData() 在首个 null 截断，
    // 避免 QString 包含 U+0000 导致路径末尾出现方框字符
    const QVariant devVar = blockIface.property("Device");
    if (devVar.isValid() && devVar.userType() == QMetaType::QByteArray) {
        info.deviceFile = QString::fromUtf8(devVar.toByteArray().constData());
    } else {
        info.deviceFile = devVar.toString();
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
            // MountPoints 为 aay（每项带尾部 null），用 constData() 截断
            info.mountPoint = QString::fromUtf8(list.first().constData());
            info.isMounted = !info.mountPoint.isEmpty();
        }
    }

    // label：已挂载时优先用 QStorageInfo（更可靠），否则用 IdLabel
    if (info.isMounted) {
        const QStorageInfo si(info.mountPoint);
        info.label = si.name();
    }
    if (info.label.isEmpty()) {
        info.label = blockIface.property("IdLabel").toString();
    }

    // 标记是否外部设备，并补充 label（型号）
    const QString drivePath = blockIface.property("Drive").value<QDBusObjectPath>().path();
    if (!drivePath.isEmpty() && drivePath != "/") {
        QDBusInterface driveIface(kUDisks2Service, drivePath, kDriveIface,
                                    QDBusConnection::systemBus());
        if (driveIface.isValid()) {
            const bool removable = driveIface.property("Removable").toBool();
            info.isRemovable = removable;
            info.isExternal = removable;
            // 仍无 label 时用驱动器型号补充
            if (info.label.isEmpty()) {
                info.label = driveIface.property("Model").toString();
            }
        }
    }

    // 选择图标
    info.icon = QIcon::fromTheme(info.isRemovable ? QStringLiteral("drive-removable-media")
                                                    : QStringLiteral("drive-harddisk"));

    return info;
}

QList<VolumeInfo> VolumeManager::listVolumes() {
    QList<VolumeInfo> result;
    for (const QStorageInfo &si : QStorageInfo::mountedVolumes()) {
        if (!si.isReady()) continue;
        const QString mp = si.rootPath();
        if (mp == QStringLiteral("/")) continue;
        const QString dev = QString::fromUtf8(si.device());
        if (!dev.startsWith(QStringLiteral("/dev/"))) continue;  // 跳过伪文件系统

        VolumeInfo info;
        info.deviceFile = dev;
        info.mountPoint = mp;
        info.label = si.name();
        info.fsType = QString::fromUtf8(si.fileSystemType());
        info.isMounted = true;
        info.icon = QIcon::fromTheme(QStringLiteral("drive-harddisk"));
        result.append(info);
    }
    return result;
}

QList<VolumeInfo> VolumeManager::listExternalDevices() {
    QList<VolumeInfo> result;
    const QStringList blockPaths = enumerateBlockDevices();
    for (const QString &bp : blockPaths) {
        VolumeInfo info = getBlockDeviceProperties(bp);
        if (info.deviceFile.isEmpty()) continue;
        // 仅保留外部（可移动）设备；仅含文件系统的块设备
        if (!info.isExternal) continue;
        if (info.fsType.isEmpty()) continue;
        result.append(info);
    }
    return result;
}

QString VolumeManager::mount(const QString &devicePath, QString *errorMsg) {
    const QString objPath = toUDisks2ObjectPath(devicePath);
    QDBusInterface fsIface(kUDisks2Service, objPath, kFsWithIface,
                            QDBusConnection::systemBus());
    if (!fsIface.isValid()) {
        if (errorMsg) *errorMsg = tr("Invalid device path: %1").arg(devicePath);
        return {};
    }
    // Mount(args) 返回挂载点字符串
    QDBusReply<QString> reply = fsIface.call(QStringLiteral("Mount"), QVariantMap{});
    if (!reply.isValid()) {
        if (errorMsg) *errorMsg = reply.error().message();
        return {};
    }
    emit volumesChanged();
    return reply.value();
}

bool VolumeManager::unmount(const QString &devicePath, QString *errorMsg) {
    const QString objPath = toUDisks2ObjectPath(devicePath);
    QDBusInterface fsIface(kUDisks2Service, objPath, kFsWithIface,
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
    const QString objPath = toUDisks2ObjectPath(devicePath);
    // Eject 需要通过 Drive 接口
    QDBusInterface blockIface(kUDisks2Service, objPath, kBlockIface,
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
