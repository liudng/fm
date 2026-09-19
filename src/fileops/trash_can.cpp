#include "trash_can.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextStream>
#include <QUrl>

#include <sys/stat.h>
#include <unistd.h>

#include <climits>

namespace fm {

namespace {
// URL 编码（用于 .trashinfo Path 字段）
QString encodeUrl(const QString &path)
{
    return QUrl::fromLocalFile(path).toEncoded(QUrl::FullyEncoded);
}

// 按 UTF-8 字节数截断（不切断多字节字符）
QString truncateUtf8(const QString &s, int maxBytes)
{
    if (s.toUtf8().size() <= maxBytes) return s;
    QString t = s;
    while (t.toUtf8().size() > maxBytes && !t.isEmpty())
        t.chop(1);
    return t;
}

// 目录所在文件系统的单名上限（NAME_MAX），获取失败回退 255
int nameMaxFor(const QString &dir)
{
    const long v = ::pathconf(QFile::encodeName(dir).constData(), _PC_NAME_MAX);
    return (v > 0 && v <= INT_MAX) ? int(v) : 255;
}
} // namespace

bool TrashCan::ensureTrashDir(const QString &trashDir, QString *errorMsg)
{
    QDir dir(trashDir);
    if (!dir.exists() && !dir.mkpath(trashDir)) {
        if (errorMsg) *errorMsg = TrashCan::tr("Cannot create trash directory: %1").arg(trashDir);
        return false;
    }
    if (!dir.mkpath(QStringLiteral("files"))) {
        if (errorMsg) *errorMsg = TrashCan::tr("Cannot create trash/files: %1").arg(trashDir);
        return false;
    }
    if (!dir.mkpath(QStringLiteral("info"))) {
        if (errorMsg) *errorMsg = TrashCan::tr("Cannot create trash/info: %1").arg(trashDir);
        return false;
    }
    return true;
}

QString TrashCan::trashDirForFile(const QString &filePath)
{
    const QString homeTrash =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        QStringLiteral("/Trash");

    // 检查文件是否在主分区
    const QStorageInfo fileStorage(filePath);
    const QStorageInfo homeStorage(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    if (fileStorage == homeStorage) {
        return homeTrash;
    }

    // 外部分区：优先使用 .Trash/<uid>（FreeDesktop.org 标准），回退到 .Trash-<uid>
    const auto uid = getuid();
    const QString deviceRoot = fileStorage.rootPath();
    const QString topTrash = deviceRoot + QStringLiteral("/.Trash");

    // 检查 .Trash 是否满足安全条件：
    // 1. 存在且是目录
    // 2. 不是符号链接
    // 3. 设置了 sticky bit（防止其他用户篡改 uid 子目录）
    QFileInfo trashInfo(topTrash);
    if (trashInfo.exists() && trashInfo.isDir() && !trashInfo.isSymLink()) {
        struct stat st;
        if (stat(topTrash.toUtf8().constData(), &st) == 0 && (st.st_mode & S_ISVTX)) {
            // .Trash 安全条件满足，使用 .Trash/<uid>
            return topTrash + QDir::separator() + QString::number(uid);
        }
    }

    // 回退到 .Trash-<uid>（无需 root，应用自行创建）
    return deviceRoot + QStringLiteral("/.Trash-%1").arg(uid);
}

QString TrashCan::uniqueTrashName(const QString &trashFilesDir, const QString &originalName)
{
    // .trashinfo 文件名 = 目标名 + ".trashinfo"；超过文件系统单名上限时
    // 截断原名并追加 hash 后缀（files/ 与 info/ 使用相同名字保持配对，
    // 恢复时按 .trashinfo 的 Path 字段记录的原路径）
    const int nameMax = nameMaxFor(trashFilesDir);
    const int infoSuffix = 10; // ".trashinfo"
    QString base = originalName;
    if (base.toUtf8().size() + infoSuffix > nameMax) {
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(originalName.toUtf8(), QCryptographicHash::Sha1).toHex().left(8));
        const QString tag = QLatin1Char('_') + hash;
        base = truncateUtf8(base, nameMax - infoSuffix - int(tag.toUtf8().size())) + tag;
    }

    QString candidate = base;
    int counter = 1;
    while (QFileInfo::exists(trashFilesDir + QDir::separator() + candidate)) {
        // _N 插入扩展名前（file_1.txt），并保证总长不超上限
        const int dot = base.lastIndexOf(QLatin1Char('.'));
        const QString num = QStringLiteral("_%1").arg(counter);
        const QString stem = dot > 0 ? base.left(dot) : base;
        const QString ext = dot > 0 ? base.mid(dot) : QString();
        candidate = truncateUtf8(stem, nameMax - int(num.toUtf8().size()) - int(ext.toUtf8().size())) +
                    num + ext;
        ++counter;
    }
    return candidate;
}

bool TrashCan::writeTrashInfo(const QString &infoPath, const QString &originalPath,
                              const QDateTime &deletionTime, QString *error)
{
    QFile f(infoPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = TrashCan::tr("Cannot write trash info: %1 (%2)").arg(infoPath, f.errorString());
        return false;
    }

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << "[Trash Info]\n";
    ts << "Path=" << encodeUrl(originalPath) << "\n";
    ts << "DeletionDate=" << deletionTime.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss")) << "\n";
    return true;
}

bool TrashCan::moveToTrash(const QUrl &fileUrl, QString *errorMsg)
{
    if (!fileUrl.isLocalFile()) {
        if (errorMsg) *errorMsg = TrashCan::tr("Only local files are supported");
        return false;
    }
    const QString filePath = fileUrl.toLocalFile();
    const QFileInfo fi(filePath);
    if (!fi.exists()) {
        if (errorMsg) *errorMsg = TrashCan::tr("File does not exist: %1").arg(filePath);
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
    {
        QString infoErr;
        if (!writeTrashInfo(infoPath, fi.absoluteFilePath(), QDateTime::currentDateTime(), &infoErr)) {
            if (errorMsg) *errorMsg = infoErr;
            return false;
        }
    }

    // 移动文件
    const QString targetPath = filesDir + QDir::separator() + trashName;
    QFile file(fi.absoluteFilePath());
    if (!file.rename(targetPath)) {
        // rename 失败可能是跨设备，尝试复制+删除
        if (!file.copy(targetPath)) {
            if (errorMsg)
                *errorMsg =
                    TrashCan::tr("Cannot move to trash: %1 (%2)").arg(filePath, file.errorString());
            // 删除已写的 info
            QFile::remove(infoPath);
            return false;
        }
        // 复制成功后删除源
        if (!file.remove()) {
            if (errorMsg)
                *errorMsg = TrashCan::tr("Cannot remove source after copy: %1 (%2)")
                                .arg(filePath, file.errorString());
            return false;
        }
    }

    return true;
}

} // namespace fm
