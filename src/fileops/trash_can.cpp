#include "trash_can.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextStream>
#include <QUrl>

#include <unistd.h>

namespace fm {

namespace {
// URL 编码（用于 .trashinfo Path 字段）
QString encodeUrl(const QString &path) {
    return QUrl::fromLocalFile(path).toEncoded(QUrl::FullyEncoded);
}
}

bool TrashCan::ensureTrashDir(const QString &trashDir, QString *errorMsg) {
    QDir dir(trashDir);
    if (!dir.exists() && !dir.mkpath(trashDir)) {
        if (errorMsg) *errorMsg = QObject::tr("Cannot create trash directory: %1").arg(trashDir);
        return false;
    }
    if (!dir.mkpath(QStringLiteral("files"))) {
        if (errorMsg) *errorMsg = QObject::tr("Cannot create trash/files: %1").arg(trashDir);
        return false;
    }
    if (!dir.mkpath(QStringLiteral("info"))) {
        if (errorMsg) *errorMsg = QObject::tr("Cannot create trash/info: %1").arg(trashDir);
        return false;
    }
    return true;
}

QString TrashCan::trashDirForFile(const QString &filePath) {
    const QString homeTrash = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                              + QStringLiteral("/Trash");

    // 检查文件是否在主分区
    const QStorageInfo fileStorage(filePath);
    const QStorageInfo homeStorage(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    if (fileStorage == homeStorage) {
        return homeTrash;
    }

    // 外部分区：使用 .Trash-1000（UID）
    const auto uid = getuid();
    QString deviceRoot = fileStorage.rootPath();
    QString topTrash = deviceRoot + QStringLiteral("/.Trash");
    QString userTrash = deviceRoot + QStringLiteral("/.Trash-%1").arg(uid);

    // 优先使用 .Trash-<uid>（无需 root，更安全）
    return userTrash;
}

QString TrashCan::uniqueTrashName(const QString &trashFilesDir, const QString &originalName) {
    QString candidate = originalName;
    int counter = 1;
    while (QFileInfo::exists(trashFilesDir + QDir::separator() + candidate)) {
        const int dot = originalName.lastIndexOf(QLatin1Char('.'));
        if (dot > 0) {
            candidate = originalName.left(dot) + QStringLiteral("_%1").arg(counter)
                        + originalName.mid(dot);
        } else {
            candidate = originalName + QStringLiteral("_%1").arg(counter);
        }
        ++counter;
    }
    return candidate;
}

bool TrashCan::writeTrashInfo(const QString &infoPath, const QString &originalPath,
                                const QDateTime &deletionTime) {
    QFile f(infoPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << "[Trash Info]\n";
    ts << "Path=" << encodeUrl(originalPath) << "\n";
    ts << "DeletionDate=" << deletionTime.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss")) << "\n";
    return true;
}

bool TrashCan::moveToTrash(const QUrl &fileUrl, QString *errorMsg) {
    if (!fileUrl.isLocalFile()) {
        if (errorMsg) *errorMsg = QObject::tr("Only local files are supported");
        return false;
    }
    const QString filePath = fileUrl.toLocalFile();
    const QFileInfo fi(filePath);
    if (!fi.exists()) {
        if (errorMsg) *errorMsg = QObject::tr("File does not exist: %1").arg(filePath);
        return false;
    }

    const QString trashDir = trashDirForFile(filePath);
    if (!ensureTrashDir(trashDir, errorMsg)) return false;

    const QString filesDir = trashDir + QStringLiteral("/files");
    const QString infoDir = trashDir + QStringLiteral("/info");

    const QString baseName = fi.fileName();
    const QString trashName = uniqueTrashName(filesDir, baseName);

    // 写 .trashinfo（先写 info 再移文件）
    const QString infoPath = infoDir + QDir::separator() + trashName + QStringLiteral(".trashinfo");
    if (!writeTrashInfo(infoPath, fi.absoluteFilePath(), QDateTime::currentDateTime())) {
        if (errorMsg) *errorMsg = QObject::tr("Cannot write trash info: %1").arg(infoPath);
        return false;
    }

    // 移动文件
    const QString targetPath = filesDir + QDir::separator() + trashName;
    QFile file(fi.absoluteFilePath());
    if (!file.rename(targetPath)) {
        // rename 失败可能是跨设备，尝试复制+删除
        if (!QFile::copy(fi.absoluteFilePath(), targetPath)) {
            if (errorMsg) *errorMsg = QObject::tr("Cannot move to trash: %1").arg(filePath);
            // 删除已写的 info
            QFile::remove(infoPath);
            return false;
        }
        // 复制成功后删除源
        if (!QFile::remove(fi.absoluteFilePath())) {
            if (errorMsg) *errorMsg = QObject::tr("Cannot remove source after copy: %1").arg(filePath);
            return false;
        }
    }

    return true;
}

bool TrashCan::moveToTrash(const QList<QUrl> &fileUrls, QString *errorMsg) {
    for (const QUrl &u : fileUrls) {
        if (!moveToTrash(u, errorMsg)) return false;
    }
    return true;
}

} // namespace fm
