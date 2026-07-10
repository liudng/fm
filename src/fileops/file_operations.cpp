#include "file_operations.h"

#include "copy_move_job.h"
#include "file_job.h"

#include "../core/clipboard_manager.h"
#include "../core/open_with_manager.h"
#include "../dialogs/conflict_dialog.h" // ConflictResolution (for qRegisterMetaType)
#include "../dialogs/error_dialog.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

namespace fm {

FileOperations *FileOperations::instance()
{
    static FileOperations inst;
    return &inst;
}

FileOperations::FileOperations(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<ConflictResolution>("fm::ConflictResolution");
}

void FileOperations::copy(const QList<QUrl> &sources, const QString &destDir)
{
    auto *job = new CopyMoveJob(sources, destDir, false, this);
    connect(job, &FileJob::directoryChanged, this, &FileOperations::directoryChanged);
    connect(job, &FileJob::completed, this, &FileOperations::operationCompleted);
    connect(job, &FileJob::failed, this, &FileOperations::operationFailed);
    job->start();
}

void FileOperations::move(const QList<QUrl> &sources, const QString &destDir)
{
    auto *job = new CopyMoveJob(sources, destDir, true, this);
    connect(job, &FileJob::directoryChanged, this, &FileOperations::directoryChanged);
    connect(job, &FileJob::completed, this, &FileOperations::operationCompleted);
    connect(job, &FileJob::failed, this, &FileOperations::operationFailed);
    job->start();
}

void FileOperations::trash(const QList<QUrl> &sources)
{
    auto *job = new TrashJob(sources, this);
    connect(job, &FileJob::directoryChanged, this, &FileOperations::directoryChanged);
    connect(job, &FileJob::completed, this, &FileOperations::operationCompleted);
    connect(job, &FileJob::failed, this, &FileOperations::operationFailed);
    job->start();
}

void FileOperations::deletePermanently(const QList<QUrl> &sources)
{
    auto *job = new DeleteJob(sources, this);
    connect(job, &FileJob::directoryChanged, this, &FileOperations::directoryChanged);
    connect(job, &FileJob::completed, this, &FileOperations::operationCompleted);
    connect(job, &FileJob::failed, this, &FileOperations::operationFailed);
    job->start();
}

void FileOperations::pasteFromClipboard(const QString &destDir)
{
    auto *clip = ClipboardManager::instance();
    if (!clip->hasFiles()) {
        ErrorDialog::show(nullptr, tr("Clipboard is empty."));
        return;
    }

    const QList<QUrl> urls = clip->files();
    if (clip->mode() == ClipboardManager::Mode::Cut) {
        move(urls, destDir);
        clip->clearCutMarks();
    } else {
        copy(urls, destDir);
    }
}

void FileOperations::rename(const QUrl &target, const QString &newName)
{
    const QString src = target.toLocalFile();
    const QFileInfo fi(src);
    const QString dst = fi.absolutePath() + QDir::separator() + newName;

    if (QFileInfo::exists(dst)) {
        ErrorDialog::show(nullptr, tr("Name already exists: %1").arg(newName));
        return;
    }
    if (!QFile::rename(src, dst)) {
        ErrorDialog::show(nullptr, tr("Cannot rename: %1").arg(src));
        return;
    }
    emit directoryChanged(fi.absolutePath());
    emit operationCompleted();
}

void FileOperations::createFile(const QString &dir, const QString &name)
{
    const QString path = dir + QDir::separator() + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        ErrorDialog::show(nullptr, tr("Cannot create file: %1").arg(path));
        return;
    }
    f.close();
    emit directoryChanged(dir);
    emit operationCompleted();
}

void FileOperations::createDir(const QString &dir, const QString &name)
{
    const QString path = dir + QDir::separator() + name;
    if (!QDir().mkdir(path)) {
        ErrorDialog::show(nullptr, tr("Cannot create folder: %1").arg(path));
        return;
    }
    emit directoryChanged(dir);
    emit operationCompleted();
}

void FileOperations::openWithDefault(const QUrl &file)
{
    const QString path = file.toLocalFile();
    const QFileInfo fi(path);
    if (!fi.isFile()) {
        QDesktopServices::openUrl(file);
        return;
    }

    // 查询文件的 MIME 类型
    QMimeDatabase db;
    const QString mimeType = db.mimeTypeForFile(fi).name();

    // 检查 [OpenWith] 是否记住选择
    const QString remembered = OpenWithManager::instance()->defaultApplication(mimeType);
    if (!remembered.isEmpty()) {
        if (QFileInfo(remembered).isAbsolute() && remembered.endsWith(QStringLiteral(".desktop"))) {
            openWithApplication(file, remembered);
        } else {
            openWithCommand(file, remembered);
        }
        return;
    }

    // 否则用 QDesktopServices（由 xdg-open 处理）
    QDesktopServices::openUrl(file);
}

void FileOperations::openWithApplication(const QUrl &file, const QString &desktopFile)
{
    // 解析 .desktop 文件的 Exec 字段
    QFile f(desktopFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ErrorDialog::show(nullptr, tr("Cannot open: %1").arg(desktopFile));
        return;
    }
    QTextStream ts(&f);
    QString exec;
    bool inDesktopEntry = false;
    while (ts.readLineInto(&exec)) {
        if (exec.startsWith(QLatin1String("["))) {
            inDesktopEntry = (exec == QStringLiteral("[Desktop Entry]"));
            continue;
        }
        if (!inDesktopEntry) continue;
        if (exec.startsWith(QLatin1String("Exec="))) {
            exec = exec.mid(5);
            break;
        }
    }
    if (exec.isEmpty()) {
        ErrorDialog::show(nullptr, tr("Invalid .desktop file: %1").arg(desktopFile));
        return;
    }

    // 替换 %f/%F/%u/%U 等占位符为文件路径
    const QString path = file.toLocalFile();
    QString cmd = exec;
    cmd.replace(QStringLiteral("%f"), QStringLiteral("\"%1\"").arg(path));
    cmd.replace(QStringLiteral("%F"), QStringLiteral("\"%1\"").arg(path));
    cmd.replace(QStringLiteral("%u"), QStringLiteral("\"%1\"").arg(path));
    cmd.replace(QStringLiteral("%U"), QStringLiteral("\"%1\"").arg(path));
    // 移除可能残留的占位符
    cmd.remove(QRegularExpression(QStringLiteral("%[a-zA-Z]")));

    QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
}

void FileOperations::openWithCommand(const QUrl &file, const QString &command)
{
    const QString path = file.toLocalFile();
    QString cmd = command;
    cmd.replace(QStringLiteral("%f"), QStringLiteral("\"%1\"").arg(path));
    cmd.replace(QStringLiteral("%F"), QStringLiteral("\"%1\"").arg(path));
    cmd.replace(QStringLiteral("%u"), QStringLiteral("\"%1\"").arg(path));
    cmd.replace(QStringLiteral("%U"), QStringLiteral("\"%1\"").arg(path));
    cmd.remove(QRegularExpression(QStringLiteral("%[a-zA-Z]")));

    // 若命令中未含文件路径占位符，追加路径
    if (!cmd.contains(path)) {
        cmd += QStringLiteral(" \"%1\"").arg(path);
    }

    QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
}

} // namespace fm
