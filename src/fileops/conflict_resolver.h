#ifndef FM_FILEOPS_CONFLICT_RESOLVER_H
#define FM_FILEOPS_CONFLICT_RESOLVER_H

#include "../dialogs/conflict_dialog.h"

#include <QObject>
#include <QString>
#include <QUrl>

namespace fm {

// 冲突解决器（独立类，便于在文件操作中复用）
// - 单次会话内支持"全部"模式记忆
// - 通过 BlockingQueuedConnection 在工作线程调用，主线程弹对话框
class ConflictResolver : public QObject
{
    Q_OBJECT
public:
    explicit ConflictResolver(QObject *parent = nullptr);

    // 解决单次冲突
    // - allowBatch=true 时允许返回 *All 变体
    // - 内部检查是否有批量记忆
    ConflictResolution resolve(const QUrl &source, const QString &destPath, bool allowBatch = true);

    // 重置批量记忆（每次新操作开始时调用）
    void resetBatchMode();

private:
    ConflictResolution batchResolution_ = ConflictResolution::Cancel;
    bool hasBatchResolution_ = false;
};

} // namespace fm

#endif // FM_FILEOPS_CONFLICT_RESOLVER_H
