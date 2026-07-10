#include "conflict_resolver.h"

#include "../dialogs/conflict_dialog.h"

#include <QThread>

namespace fm {

ConflictResolver::ConflictResolver(QObject *parent) : QObject(parent) {}

void ConflictResolver::resetBatchMode()
{
    batchResolution_ = ConflictResolution::Cancel;
    hasBatchResolution_ = false;
}

ConflictResolution ConflictResolver::resolve(const QUrl &source, const QString &destPath,
                                             bool allowBatch)
{
    // 若已有批量记忆且非 Rename（Rename 需要每次输入新名）
    if (hasBatchResolution_ && batchResolution_ != ConflictResolution::RenameAll) {
        return batchResolution_;
    }

    const QString sourceName = source.fileName();

    // 实际弹对话框的逻辑（必须在主线程执行）
    auto showDialog = [this, &sourceName, &destPath, allowBatch]() {
        ConflictDialog dlg(sourceName, destPath, allowBatch, nullptr);
        dlg.exec();
        const ConflictResolution r = dlg.resolution();
        if (r == ConflictResolution::OverwriteAll || r == ConflictResolution::SkipAll ||
            r == ConflictResolution::RenameAll) {
            batchResolution_ = r;
            hasBatchResolution_ = true;
        }
        return r;
    };

    // 主线程直接调用；工作线程用 BlockingQueuedConnection 阻塞等待
    if (this->thread() == QThread::currentThread()) {
        return showDialog();
    }
    // 工作线程：阻塞等待主线程执行对话框
    ConflictResolution r = ConflictResolution::Cancel;
    QMetaObject::invokeMethod(
        this, [&r, &showDialog]() { r = showDialog(); }, Qt::BlockingQueuedConnection);
    return r;
}

} // namespace fm
