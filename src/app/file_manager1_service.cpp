#include "file_manager1_service.h"

#include <QDBusConnection>
#include <QUrl>
#include <QDebug>

namespace fm {

namespace {
constexpr auto kServiceName = "org.freedesktop.FileManager1";
constexpr auto kObjectPath = "/org/freedesktop/FileManager1";
} // namespace

FileManager1Service::FileManager1Service(QObject *parent) : QObject(parent) {}

bool FileManager1Service::registerService()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "FileManager1: session bus not available, D-Bus service not registered";
        return false;
    }
    if (!bus.registerService(QString::fromLatin1(kServiceName))) {
        qWarning() << "FileManager1: failed to acquire" << kServiceName
                   << "(owned by another file manager?)";
        return false;
    }
    if (!bus.registerObject(QString::fromLatin1(kObjectPath), this,
                            QDBusConnection::ExportAllSlots)) {
        qWarning() << "FileManager1: failed to register object" << kObjectPath;
        bus.unregisterService(QString::fromLatin1(kServiceName));
        return false;
    }
    return true;
}

void FileManager1Service::ShowFolders(const QStringList &uris, const QString &startupId)
{
    Q_UNUSED(startupId); // XDG startup notification ID，当前不使用
    const QStringList paths = urisToLocalPaths(uris);
    if (paths.isEmpty()) return;
    emit showFoldersRequested(paths);
}

void FileManager1Service::ShowItems(const QStringList &uris, const QString &startupId)
{
    Q_UNUSED(startupId);
    const QStringList paths = urisToLocalPaths(uris);
    if (paths.isEmpty()) return;
    emit showItemsRequested(paths);
}

void FileManager1Service::ShowItemProperties(const QStringList &uris, const QString &startupId)
{
    Q_UNUSED(startupId);
    const QStringList paths = urisToLocalPaths(uris);
    if (paths.isEmpty()) return;
    emit showItemPropertiesRequested(paths);
}

QStringList FileManager1Service::urisToLocalPaths(const QStringList &uris)
{
    QStringList paths;
    for (const QString &u : uris) {
        const QUrl url(u);
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        } else if (url.scheme().isEmpty() && u.startsWith(QLatin1Char('/'))) {
            // 容错：部分调用方直接传绝对路径而非 file:// URI
            paths.append(u);
        }
    }
    return paths;
}

} // namespace fm
