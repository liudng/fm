#include "copy_move_job.h"

#include "progress_dialog.h"

#include "../core/config_manager.h"
#include "../dialogs/error_dialog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

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
                    FileJob *job, const QString &fileName) {
    QFile srcFile(src);
    if (!srcFile.open(QIODevice::ReadOnly)) return -1;
    QFile dstFile(dst);
    if (!dstFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        srcFile.close();
        return -1;
    }
    // 分块大小从配置读取（默认 1MB），可按存储介质类型调整
    const qint64 chunkSize = static_cast<qint64>(
        ConfigManager::instance()->value(QStringLiteral("File_Operations"),
            QStringLiteral("chunkSizeMB"), 1).toInt()) * 1024 * 1024;
    QByteArray buffer;
    buffer.resize(static_cast<int>(chunkSize));
    while (!srcFile.atEnd()) {
        // 检查取消标志
        if (job->isCanceled()) {
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
        job->reportProgress(percent, fileName);
    }
    srcFile.close();
    dstFile.close();
    return 0;
}

// 递归复制目录（带进度报告）
// 返回值：0=成功, 1=用户取消, -1=失败
int copyRecursively(const QString &src, const QString &dst, QString *error,
                    qint64 *processedBytes, qint64 totalBytes,
                    FileJob *job) {
    QFileInfo srcInfo(src);
    if (srcInfo.isFile()) {
        const QString name = srcInfo.fileName();
        const int ret = copyFileChunked(src, dst, processedBytes, totalBytes, job, name);
        if (ret != 0) {
            if (ret == -1 && error) *error = QObject::tr("Cannot copy: %1 -> %2").arg(src, dst);
            return ret;
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
        const int ret = copyRecursively(s, d, error, processedBytes, totalBytes, job);
        if (ret != 0) return ret;
    }
    return 0;
}

} // namespace

CopyMoveJob::CopyMoveJob(const QList<QUrl> &sources, const QString &destDir, bool isMove,
                           QObject *parent)
    : FileJob(parent), sources_(sources), destDir_(destDir), isMove_(isMove),
      resolver_(this) {
}

bool CopyMoveJob::prepare() {
    resolver_.resetBatchMode();

    // 在主线程预先检查所有冲突，生成目标名映射。
    // 必须在创建 ProgressDialog 之前完成：showDelayed() 的定时器会在
    // ConflictDialog::exec() 的本地事件循环中触发，导致进度对话框
    // 覆盖在冲突对话框之上，使其无法操作。
    bool userCanceled = false;  // 用户点击冲突对话框"取消"
    for (const QUrl &u : sources_) {
        const QString src = u.toLocalFile();
        const QString name = QFileInfo(src).fileName();
        QString dst = destDir_ + QDir::separator() + name;

        // 防止将文件夹复制/移动到自身或其子目录中（会导致无限递归）
        if (QFileInfo(src).isDir()) {
            const QString srcCanon = QDir(src).canonicalPath();
            const QString dstCanon = QDir(destDir_).canonicalPath();
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
            const QString newName = uniqueName(destDir_, name);
            dst = destDir_ + QDir::separator() + newName;
            plan_.append({src, dst});
            continue;
        }

        if (QFileInfo::exists(dst)) {
            ConflictResolution r = resolver_.resolve(u, dst, sources_.size() > 1);
            if (r == ConflictResolution::Cancel) {
                // 取消整个粘贴操作
                userCanceled = true;
                break;
            }
            if (r == ConflictResolution::Skip ||
                r == ConflictResolution::SkipAll) continue;
            if (r == ConflictResolution::Rename ||
                r == ConflictResolution::RenameAll) {
                QString newName = uniqueName(destDir_, name);
                dst = destDir_ + QDir::separator() + newName;
            }
            // Overwrite / OverwriteAll: 删除目标
            if (r == ConflictResolution::Overwrite ||
                r == ConflictResolution::OverwriteAll) {
                removeRecursively(dst, nullptr);
            }
        }
        plan_.append({src, dst});
    }

    // 用户在冲突对话框中点击了"取消"，或没有文件需要处理：直接结束
    if (userCanceled) {
        emit completed();  // 触发目标目录刷新
        return false;
    }
    if (plan_.isEmpty()) {
        return false;
    }

    // 冲突解决完毕后创建进度对话框（避免 showDelayed 在冲突对话框事件循环中触发）
    auto *pd = new ProgressDialog(nullptr);
    pd->setOperationTitle(isMove_ ? tr("Moving...") : tr("Copying..."));
    pd->showDelayed();
    setProgressDialog(pd);
    connect(pd, &ProgressDialog::canceled, this, &CopyMoveJob::cancel);

    // 预计算总字节数（用于进度显示）
    totalBytes_ = 0;
    for (const auto &p : plan_) {
        totalBytes_ += countBytes(p.first);
    }

    return true;
}

bool CopyMoveJob::execute(QString *error) {
    qint64 processedBytes = 0;
    for (const auto &p : plan_) {
        // 检查取消标志
        if (isCanceled()) break;

        if (isMove_) {
            if (!QFile::rename(p.first, p.second)) {
                // 跨设备，复制+删除
                const int ret = copyRecursively(p.first, p.second, error,
                                                  &processedBytes, totalBytes_, this);
                if (ret == 1) break;  // 用户取消，保留已复制部分
                if (ret == -1) return false;
                if (!removeRecursively(p.first, error)) return false;
            }
        } else {
            const int ret = copyRecursively(p.first, p.second, error,
                                              &processedBytes, totalBytes_, this);
            if (ret == 1) break;  // 用户取消，保留已复制部分
            if (ret == -1) return false;
        }
    }
    return true;  // 即使取消也返回 true（部分成功，不回滚）
}

QStringList CopyMoveJob::affectedDirectories() const {
    QStringList dirs;
    if (isMove_) {
        // 移动后需要刷新源目录
        for (const QUrl &u : sources_) {
            dirs.append(QFileInfo(u.toLocalFile()).absolutePath());
        }
    }
    dirs.append(destDir_);
    return dirs;
}

QString CopyMoveJob::uniqueName(const QString &dir, const QString &name) {
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

} // namespace fm
