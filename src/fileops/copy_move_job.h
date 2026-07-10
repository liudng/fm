#ifndef FM_FILEOPS_COPY_MOVE_JOB_H
#define FM_FILEOPS_COPY_MOVE_JOB_H

#include "conflict_resolver.h"
#include "file_job.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

namespace fm {

// 复制/移动作业
// - prepare()：主线程预扫描冲突（委托 ConflictResolver 弹 ConflictDialog），创建 ProgressDialog
// - execute()：工作线程执行复制/移动，支持分块进度、跨设备移动（复制+删除）
// - 取消：保留已复制部分不回滚
class CopyMoveJob : public FileJob
{
    Q_OBJECT
public:
    CopyMoveJob(const QList<QUrl> &sources, const QString &destDir, bool isMove,
                QObject *parent = nullptr);

protected:
    bool prepare() override;
    bool execute(QString *error) override;
    QStringList affectedDirectories() const override;

private:
    static QString uniqueName(const QString &dir, const QString &name);

    QList<QUrl> sources_;
    QString destDir_;
    bool isMove_;

    // 复制计划（src -> dst），在 prepare() 中构建
    QList<QPair<QString, QString>> plan_;
    qint64 totalBytes_ = 0;

    ConflictResolver resolver_; // 冲突解决器（含批量记忆）
};

} // namespace fm

#endif // FM_FILEOPS_COPY_MOVE_JOB_H
