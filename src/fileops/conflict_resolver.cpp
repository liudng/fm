#include "conflict_resolver.h"

#include "../dialogs/conflict_dialog.h"

namespace fm {

ConflictResolver::ConflictResolver(QObject *parent)
    : QObject(parent) {
}

void ConflictResolver::resetBatchMode() {
    batchResolution_ = ConflictResolution::Cancel;
    hasBatchResolution_ = false;
}

ConflictResolution ConflictResolver::resolve(const QUrl &source, const QString &destPath,
                                                 bool allowBatch) {
    // 若已有批量记忆且非 Rename（Rename 需要每次输入新名）
    if (hasBatchResolution_ && batchResolution_ != ConflictResolution::RenameAll) {
        return batchResolution_;
    }

    ConflictResolution r = ConflictResolution::Cancel;
    const QString sourceName = source.fileName();

    // 在主线程同步执行对话框
    QMetaObject::invokeMethod(this, [&, this]() {
        ConflictDialog dlg(sourceName, destPath, allowBatch, nullptr);
        dlg.exec();
        r = dlg.resolution();
        if (r == ConflictResolution::OverwriteAll ||
            r == ConflictResolution::SkipAll ||
            r == ConflictResolution::RenameAll) {
            batchResolution_ = r;
            hasBatchResolution_ = true;
        }
    }, Qt::BlockingQueuedConnection);

    // RenameAll 在批量场景下，后续仍需弹对话框输入新名
    // 但首次的 r 直接是 RenameAll，调用方需在批量场景特殊处理
    return r;
}

} // namespace fm
