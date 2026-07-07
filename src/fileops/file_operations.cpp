#include "file_operations.h"

#include "../core/clipboard_manager.h"
#include "../dialogs/conflict_dialog.h"
#include "../dialogs/error_dialog.h"
#include "../dialogs/input_name_dialog.h"
#include "../fileops/progress_dialog.h"
#include "../fileops/trash_can.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>

namespace fm {

namespace {

// 递归复制目录
bool copyRecursively(const QString &src, const QString &dst, QString *error) {
    QFileInfo srcInfo(src);
    if (srcInfo.isFile()) {
        if (!QFile::copy(src, dst)) {
            if (error) *error = QObject::tr("Cannot copy: %1 -> %2").arg(src, dst);
            return false;
        }
        return true;
    }
    QDir().mkpath(dst);
    QDir srcDir(src);
    const QStringList entries = srcDir.entryList(QDir::Files | QDir::Dirs |
                                                   QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QString &e : entries) {
        const QString s = srcDir.filePath(e);
        const QString d = dst + QDir::separator() + e;
        if (!copyRecursively(s, d, error)) return false;
    }
    return true;
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
    ConflictResolution r = ConflictResolution::Cancel;
    // 在主线程同步执行对话框（本方法已在主线程调用）
    QMetaObject::invokeMethod(this, [&, this]() {
        ConflictDialog dlg(sourceName, destPath, allowBatch, nullptr);
        dlg.exec();
        r = dlg.resolution();
        if (dlg.resolution() == ConflictResolution::OverwriteAll ||
            dlg.resolution() == ConflictResolution::SkipAll ||
            dlg.resolution() == ConflictResolution::RenameAll) {
            batchResolution_ = dlg.resolution();
            hasBatchResolution_ = true;
        }
    }, Qt::BlockingQueuedConnection);
    return r;
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

    progressDialog_ = new ProgressDialog(nullptr);
    progressDialog_->setOperationTitle(isMove ? tr("Moving...") : tr("Copying..."));
    progressDialog_->showDelayed();
    connect(progressDialog_, &ProgressDialog::canceled, watcher, &QFutureWatcher<bool>::cancel);

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

    // 复制/移动不能在 lambda 中调用主线程的 resolveConflict（会死锁）
    // 简化版：在主线程预先检查所有冲突，生成目标名映射
    QList<QPair<QString, QString>> plan;  // src -> dst
    for (const QUrl &u : sources) {
        const QString src = u.toLocalFile();
        const QString name = QFileInfo(src).fileName();
        QString dst = destDir + QDir::separator() + name;
        if (QFileInfo::exists(dst)) {
            ConflictResolution r = resolveConflict(name, dst, sources.size() > 1);
            if (r == ConflictResolution::Cancel) continue;
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

    // 异步执行
    QString *errPtr = new QString;
    *errPtr = errorMsg;
    watcher->setProperty("error", QVariant());

    auto future = QtConcurrent::run([plan, isMove, errPtr]() -> bool {
        for (const auto &p : plan) {
            if (isMove) {
                if (!QFile::rename(p.first, p.second)) {
                    // 跨设备，复制+删除
                    if (!copyRecursively(p.first, p.second, errPtr)) return false;
                    if (!removeRecursively(p.first, errPtr)) return false;
                }
            } else {
                if (!copyRecursively(p.first, p.second, errPtr)) return false;
            }
        }
        return true;
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

void FileOperations::createFile(const QString &dir, const QString &defaultName) {
    const QString name = uniqueName(dir, defaultName);
    InputNameDialog dlg(tr("New File"), tr("File name:"), name, nullptr);

    // 设置已存在名称用于校验
    QDir d(dir);
    dlg.setExistingNames(d.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot));

    if (dlg.exec() != QDialog::Accepted) return;
    const QString finalName = dlg.name();
    const QString path = dir + QDir::separator() + finalName;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        ErrorDialog::show(nullptr, tr("Cannot create file: %1").arg(path));
        return;
    }
    f.close();
    emit directoryChanged(dir);
    emit operationCompleted();
}

void FileOperations::createDir(const QString &dir, const QString &defaultName) {
    const QString name = uniqueName(dir, defaultName);
    InputNameDialog dlg(tr("New Folder"), tr("Folder name:"), name, nullptr);

    QDir d(dir);
    dlg.setExistingNames(d.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot));

    if (dlg.exec() != QDialog::Accepted) return;
    const QString finalName = dlg.name();
    const QString path = dir + QDir::separator() + finalName;

    if (!QDir().mkdir(path)) {
        ErrorDialog::show(nullptr, tr("Cannot create folder: %1").arg(path));
        return;
    }
    emit directoryChanged(dir);
    emit operationCompleted();
}

void FileOperations::openWithDefault(const QUrl &file) {
    QDesktopServices::openUrl(file);
}

} // namespace fm
