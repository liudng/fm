#include "file_job.h"

#include "progress_dialog.h"
#include "trash_can.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>

namespace fm {

// ============================================================
// FileJob
// ============================================================

FileJob::FileJob(QObject *parent)
    : QObject(parent), cancelFlag_(std::make_shared<std::atomic<bool>>(false)) {
}

FileJob::~FileJob() = default;

void FileJob::start() {
    // 主线程预处理（冲突解决、创建进度对话框等）
    if (!prepare()) {
        deleteLater();
        return;
    }

    // 异步执行
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        const bool ok = watcher->result();
        watcher->deleteLater();

        // 清理进度对话框
        if (progressDialog_) {
            delete progressDialog_;
            progressDialog_ = nullptr;
        }

        if (ok) {
            for (const QString &dir : affectedDirectories()) {
                emit directoryChanged(dir);
            }
            emit completed();
        } else {
            emit failed(error_);
        }

        // 作业完成后自动销毁
        deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([this]() -> bool {
        return execute(&error_);
    }));
}

void FileJob::cancel() {
    cancelFlag_->store(true);
}

void FileJob::reportProgress(int percent, const QString &currentFile) {
    if (!progressDialog_) return;
    // 使用 QPointer 防止进度对话框在异步投递期间被销毁导致悬空访问
    QPointer<ProgressDialog> pd = progressDialog_;
    QMetaObject::invokeMethod(pd.data(), [pd, percent, currentFile]() {
        if (pd) {
            pd->setProgress(percent);
            pd->setCurrentFile(currentFile);
        }
    }, Qt::QueuedConnection);
}

bool FileJob::removeRecursively(const QString &path, QString *error) {
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

// ============================================================
// DeleteJob
// ============================================================

DeleteJob::DeleteJob(const QList<QUrl> &sources, QObject *parent)
    : FileJob(parent), sources_(sources) {
}

bool DeleteJob::execute(QString *error) {
    for (const QUrl &u : sources_) {
        if (isCanceled()) break;
        if (!removeRecursively(u.toLocalFile(), error)) return false;
    }
    return true;
}

QStringList DeleteJob::affectedDirectories() const {
    QStringList dirs;
    for (const QUrl &u : sources_) {
        dirs.append(QFileInfo(u.toLocalFile()).absolutePath());
    }
    return dirs;
}

// ============================================================
// TrashJob
// ============================================================

TrashJob::TrashJob(const QList<QUrl> &sources, QObject *parent)
    : FileJob(parent), sources_(sources) {
}

bool TrashJob::execute(QString *error) {
    return TrashCan::moveToTrash(sources_, error);
}

QStringList TrashJob::affectedDirectories() const {
    QStringList dirs;
    for (const QUrl &u : sources_) {
        dirs.append(QFileInfo(u.toLocalFile()).absolutePath());
    }
    return dirs;
}

} // namespace fm
