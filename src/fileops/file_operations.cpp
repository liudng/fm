#include "file_operations.h"

#include "../core/clipboard_manager.h"
#include "../core/open_with_manager.h"
#include "../dialogs/conflict_dialog.h"
#include "../dialogs/error_dialog.h"
#include "../fileops/progress_dialog.h"
#include "../fileops/trash_can.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QtConcurrent>

#include <atomic>
#include <memory>

namespace fm {

namespace {

// 递归计算路径的总字节数
qint64 countBytes(const QString &path) {
    QFileInfo fi(path);
    if (fi.isFile()) return fi.size();
    qint64 total = 0;
    QDir dir(path);
    const QStringList entries = dir.entryList(QDir::Files | QDir::Dirs |
                                                QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QString &e : entries) {
        total += countBytes(dir.filePath(e));
    }
    return total;
}

// 分块复制文件并报告进度（支持大文件复制进度显示）
// 返回值：0=成功, 1=用户取消, -1=失败
int copyFileChunked(const QString &src, const QString &dst,
                    qint64 *processedBytes, qint64 totalBytes,
                    ProgressDialog *progress, const QString &fileName,
                    const std::atomic<bool> &canceled) {
    QFile srcFile(src);
    if (!srcFile.open(QIODevice::ReadOnly)) return -1;
    QFile dstFile(dst);
    if (!dstFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        srcFile.close();
        return -1;
    }
    constexpr qint64 chunkSize = 4 * 1024 * 1024;  // 4 MB
    QByteArray buffer;
    buffer.resize(static_cast<int>(chunkSize));
    while (!srcFile.atEnd()) {
        // 检查取消标志
        if (canceled.load()) {
            srcFile.close();
            dstFile.close();
            // 删除未复制完成的不完整文件（已完成的不回滚）
            QFile::remove(dst);
            return 1;  // 用户取消
        }
        const qint64 n = srcFile.read(buffer.data(), chunkSize);
        if (n <= 0) break;
        if (dstFile.write(buffer.data(), n) != n) {
            srcFile.close();
            dstFile.close();
            return -1;
        }
        *processedBytes += n;
        const int percent = (totalBytes > 0)
            ? static_cast<int>((*processedBytes * 100) / totalBytes) : 0;
        // 线程安全地更新进度对话框（工作线程 → 主线程）
        QMetaObject::invokeMethod(progress, [progress, percent, fileName]() {
            progress->setProgress(percent);
            progress->setCurrentFile(fileName);
        }, Qt::QueuedConnection);
    }
    srcFile.close();
    dstFile.close();
    return 0;
}

// 递归复制目录（带进度报告）
// 返回值：0=成功, 1=用户取消, -1=失败
int copyRecursively(const QString &src, const QString &dst, QString *error,
                    qint64 *processedBytes, qint64 totalBytes,
                    ProgressDialog *progress, const std::atomic<bool> &canceled) {
    QFileInfo srcInfo(src);
    if (srcInfo.isFile()) {
        const QString name = srcInfo.fileName();
        if (progress) {
            const int ret = copyFileChunked(src, dst, processedBytes, totalBytes, progress, name, canceled);
            if (ret != 0) {
                if (ret == -1 && error) *error = QObject::tr("Cannot copy: %1 -> %2").arg(src, dst);
                return ret;
            }
        } else {
            if (!QFile::copy(src, dst)) {
                if (error) *error = QObject::tr("Cannot copy: %1 -> %2").arg(src, dst);
                return -1;
            }
        }
        return 0;
    }
    QDir().mkpath(dst);
    QDir srcDir(src);
    const QStringList entries = srcDir.entryList(QDir::Files | QDir::Dirs |
                                                   QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QString &e : entries) {
        const QString s = srcDir.filePath(e);
        const QString d = dst + QDir::separator() + e;
        const int ret = copyRecursively(s, d, error, processedBytes, totalBytes, progress, canceled);
        if (ret != 0) return ret;
    }
    return 0;
}

// 递归删除目录
bool removeRecursively(const QString &path, QString *error) {
    QFileInfo fi(path);
    if (fi.isFile()) {
        if (!QFile::remove(path)) {
            if (error) *error = QObject::tr("Cannot remove: %1").arg(path);
            return false;
        }
        return true;
    }
    QDir dir(path);
    const QStringList entries = dir.entryList(QDir::Files | QDir::Dirs |
                                                QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QString &e : entries) {
        if (!removeRecursively(dir.filePath(e), error)) return false;
    }
    if (!dir.rmdir(path)) {
        if (error) *error = QObject::tr("Cannot rmdir: %1").arg(path);
        return false;
    }
    return true;
}

} // namespace

FileOperations *FileOperations::instance() {
    static FileOperations inst;
    return &inst;
}

FileOperations::FileOperations(QObject *parent)
    : QObject(parent),
      progressDialog_(nullptr) {
    qRegisterMetaType<ConflictResolution>("fm::ConflictResolution");
}

ConflictResolution FileOperations::resolveConflict(const QString &sourceName,
                                                    const QString &destPath,
                                                    bool allowBatch) {
    if (hasBatchResolution_) {
        // 检查是否需要弹 rename（无法批量）
        if (batchResolution_ == ConflictResolution::RenameAll) {
            // 仍需弹对话框让用户输入新名
        } else {
            return batchResolution_;
        }
    }
    // 本方法已在主线程调用，直接显示对话框
    // （之前使用 BlockingQueuedConnection 会导致同线程死锁）
    ConflictDialog dlg(sourceName, destPath, allowBatch, nullptr);
    dlg.exec();
    if (dlg.resolution() == ConflictResolution::OverwriteAll ||
        dlg.resolution() == ConflictResolution::SkipAll ||
        dlg.resolution() == ConflictResolution::RenameAll) {
        batchResolution_ = dlg.resolution();
        hasBatchResolution_ = true;
    }
    return dlg.resolution();
}

QString FileOperations::uniqueName(const QString &dir, const QString &name) {
    if (!QFileInfo::exists(dir + QDir::separator() + name)) return name;
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    QString base, ext;
    if (dot > 0) {
        base = name.left(dot);
        ext = name.mid(dot);
    } else {
        base = name;
    }
    int counter = 1;
    QString candidate;
    do {
        candidate = base + QStringLiteral("_%1").arg(counter) + ext;
        ++counter;
    } while (QFileInfo::exists(dir + QDir::separator() + candidate));
    return candidate;
}

void FileOperations::copy(const QList<QUrl> &sources, const QString &destDir) {
    runCopyMove(sources, destDir, false);
}

void FileOperations::move(const QList<QUrl> &sources, const QString &destDir) {
    runCopyMove(sources, destDir, true);
}

void FileOperations::runCopyMove(const QList<QUrl> &sources, const QString &destDir, bool isMove) {
    batchResolution_ = ConflictResolution::Cancel;
    hasBatchResolution_ = false;

    auto *watcher = new QFutureWatcher<bool>(this);
    QString errorMsg;

    // 在主线程预先检查所有冲突，生成目标名映射。
    // 必须在创建 ProgressDialog 之前完成：showDelayed() 的定时器会在
    // ConflictDialog::exec() 的本地事件循环中触发，导致进度对话框
    // 覆盖在冲突对话框之上，使其无法操作。
    QList<QPair<QString, QString>> plan;  // src -> dst
    bool userCanceled = false;  // 用户点击冲突对话框"取消"
    for (const QUrl &u : sources) {
        const QString src = u.toLocalFile();
        const QString name = QFileInfo(src).fileName();
        QString dst = destDir + QDir::separator() + name;

        // 防止将文件夹复制/移动到自身或其子目录中（会导致无限递归）
        if (QFileInfo(src).isDir()) {
            const QString srcCanon = QDir(src).canonicalPath();
            const QString dstCanon = QDir(destDir).canonicalPath();
            if (!srcCanon.isEmpty() && !dstCanon.isEmpty() &&
                (dstCanon == srcCanon ||
                 dstCanon.startsWith(srcCanon + QLatin1Char('/')))) {
                ErrorDialog::show(nullptr,
                    tr("Cannot copy folder \"%1\" into itself.").arg(name));
                continue;
            }
        }

        if (src == dst) {
            // 同文件夹粘贴（源与目标相同）：自动重命名
            const QString newName = uniqueName(destDir, name);
            dst = destDir + QDir::separator() + newName;
            plan.append({src, dst});
            continue;
        }

        if (QFileInfo::exists(dst)) {
            ConflictResolution r = resolveConflict(name, dst, sources.size() > 1);
            if (r == ConflictResolution::Cancel) {
                // 取消整个粘贴操作
                userCanceled = true;
                break;
            }
            if (r == ConflictResolution::Skip ||
                r == ConflictResolution::SkipAll) continue;
            if (r == ConflictResolution::Rename ||
                r == ConflictResolution::RenameAll) {
                QString newName = uniqueName(destDir, name);
                dst = destDir + QDir::separator() + newName;
            }
            // Overwrite / OverwriteAll: 删除目标
            if (r == ConflictResolution::Overwrite ||
                r == ConflictResolution::OverwriteAll) {
                removeRecursively(dst, nullptr);
            }
        }
        plan.append({src, dst});
    }

    // 用户在冲突对话框中点击了"取消"，或没有文件需要处理：直接结束
    if (userCanceled || plan.isEmpty()) {
        if (userCanceled) {
            emit operationCompleted();  // 刷新目标目录
        }
        delete watcher;
        return;
    }

    // 冲突解决完毕后创建进度对话框（避免 showDelayed 在冲突对话框事件循环中触发）
    progressDialog_ = new ProgressDialog(nullptr);
    progressDialog_->setOperationTitle(isMove ? tr("Moving...") : tr("Copying..."));
    progressDialog_->showDelayed();

    // 取消标志（用于进度对话框取消后通知工作线程停止）
    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
    connect(progressDialog_, &ProgressDialog::canceled, this, [cancelFlag]() {
        cancelFlag->store(true);
    });

    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, destDir, isMove]() {
        bool ok = watcher->result();
        delete progressDialog_;
        progressDialog_ = nullptr;

        if (ok) {
            // 刷新源目录与目标目录
            if (isMove) {
                for (const QUrl &u : watcher->property("sources").value<QList<QUrl>>()) {
                    emit directoryChanged(QFileInfo(u.toLocalFile()).absolutePath());
                }
            }
            emit directoryChanged(destDir);
            emit operationCompleted();
        } else {
            emit operationFailed(watcher->property("error").toString());
        }
        watcher->deleteLater();
    });

    watcher->setProperty("sources", QVariant::fromValue(sources));
    watcher->setProperty("destDir", destDir);

    // 异步执行
    QString *errPtr = new QString;
    *errPtr = errorMsg;
    watcher->setProperty("error", QVariant());

    // 预计算总字节数（用于进度显示）
    qint64 totalBytes = 0;
    for (const auto &p : plan) {
        totalBytes += countBytes(p.first);
    }

    ProgressDialog *progress = progressDialog_;  // 捕获进度对话框指针
    auto future = QtConcurrent::run([plan, isMove, errPtr, totalBytes, progress, cancelFlag]() -> bool {
        qint64 processedBytes = 0;
        for (const auto &p : plan) {
            // 检查取消标志
            if (cancelFlag->load()) break;

            if (isMove) {
                if (!QFile::rename(p.first, p.second)) {
                    // 跨设备，复制+删除
                    const int ret = copyRecursively(p.first, p.second, errPtr, &processedBytes, totalBytes, progress, *cancelFlag);
                    if (ret == 1) break;  // 用户取消，保留已复制部分
                    if (ret == -1) return false;
                    if (!removeRecursively(p.first, errPtr)) return false;
                }
            } else {
                const int ret = copyRecursively(p.first, p.second, errPtr, &processedBytes, totalBytes, progress, *cancelFlag);
                if (ret == 1) break;  // 用户取消，保留已复制部分
                if (ret == -1) return false;
            }
        }
        return true;  // 即使取消也返回 true（部分成功，不回滚）
    });
    watcher->setFuture(future);

    // 完成后取出 error
    connect(watcher, &QFutureWatcher<bool>::finished, this, [watcher, errPtr]() {
        if (!watcher->result() && errPtr) {
            watcher->setProperty("error", *errPtr);
        }
        delete errPtr;
    });
}

void FileOperations::trash(const QList<QUrl> &sources) {
    runTrash(sources);
}

void FileOperations::runTrash(const QList<QUrl> &sources) {
    auto *watcher = new QFutureWatcher<bool>(this);

    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, sources]() {
        bool ok = watcher->result();
        if (ok) {
            // 刷新源目录
            for (const QUrl &u : sources) {
                emit directoryChanged(QFileInfo(u.toLocalFile()).absolutePath());
            }
            emit operationCompleted();
        } else {
            emit operationFailed(watcher->property("error").toString());
        }
        watcher->deleteLater();
    });

    QString *errPtr = new QString;
    auto future = QtConcurrent::run([sources, errPtr]() -> bool {
        return TrashCan::moveToTrash(sources, errPtr);
    });
    watcher->setProperty("error", QVariant());

    connect(watcher, &QFutureWatcher<bool>::finished, this, [watcher, errPtr]() {
        if (!watcher->result() && errPtr) {
            watcher->setProperty("error", *errPtr);
        }
        delete errPtr;
    });
    watcher->setFuture(future);
}

void FileOperations::deletePermanently(const QList<QUrl> &sources) {
    runDelete(sources);
}

void FileOperations::runDelete(const QList<QUrl> &sources) {
    auto *watcher = new QFutureWatcher<bool>(this);
    QString *errPtr = new QString;

    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, sources, errPtr]() {
        bool ok = watcher->result();
        if (ok) {
            for (const QUrl &u : sources) {
                emit directoryChanged(QFileInfo(u.toLocalFile()).absolutePath());
            }
            emit operationCompleted();
        } else {
            emit operationFailed(*errPtr);
        }
        delete errPtr;
        watcher->deleteLater();
    });

    auto future = QtConcurrent::run([sources, errPtr]() -> bool {
        for (const QUrl &u : sources) {
            if (!removeRecursively(u.toLocalFile(), errPtr)) return false;
        }
        return true;
    });
    watcher->setFuture(future);
}

void FileOperations::pasteFromClipboard(const QString &destDir) {
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

void FileOperations::rename(const QUrl &target, const QString &newName) {
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

void FileOperations::createFile(const QString &dir, const QString &name) {
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

void FileOperations::createDir(const QString &dir, const QString &name) {
    const QString path = dir + QDir::separator() + name;
    if (!QDir().mkdir(path)) {
        ErrorDialog::show(nullptr, tr("Cannot create folder: %1").arg(path));
        return;
    }
    emit directoryChanged(dir);
    emit operationCompleted();
}

void FileOperations::openWithDefault(const QUrl &file) {
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

void FileOperations::openWithApplication(const QUrl &file, const QString &desktopFile) {
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

void FileOperations::openWithCommand(const QUrl &file, const QString &command) {
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
