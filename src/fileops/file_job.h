#ifndef FM_FILEOPS_FILE_JOB_H
#define FM_FILEOPS_FILE_JOB_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include "progress_dialog.h"

#include <atomic>
#include <memory>

namespace fm {

// 文件操作异步作业基类
// - 管理 QFutureWatcher 生命周期、取消标志、进度对话框
// - 子类实现 prepare()（主线程预处理）和 execute()（工作线程执行）
// - 完成后自动 deleteLater()
class FileJob : public QObject {
    Q_OBJECT
public:
    explicit FileJob(QObject *parent = nullptr);
    ~FileJob() override;

    // 启动作业：先在主线程调用 prepare()，若通过则异步执行 execute()
    void start();

    // 请求取消（线程安全）
    void cancel();

    // 线程安全的取消检查（工作线程调用）
    bool isCanceled() const { return cancelFlag_->load(); }

    // 线程安全的进度上报（工作线程调用）
    void reportProgress(int percent, const QString &currentFile);

protected:
    // 主线程预处理（如冲突解决、创建进度对话框）。返回 false 表示中止。
    virtual bool prepare() { return true; }

    // 工作线程执行实际操作。返回 false 表示失败。
    virtual bool execute(QString *error) = 0;

    // 成功后需要刷新的目录列表
    virtual QStringList affectedDirectories() const = 0;

    // 进度对话框
    ProgressDialog *progressDialog() const { return progressDialog_; }
    void setProgressDialog(ProgressDialog *pd) { progressDialog_ = pd; }

    // 递归删除目录（CopyMoveJob 跨设备移动和 DeleteJob 共用）
    static bool removeRecursively(const QString &path, QString *error);

signals:
    void directoryChanged(const QString &dir);
    void completed();
    void failed(const QString &error);

private:
    QPointer<ProgressDialog> progressDialog_;
    std::shared_ptr<std::atomic<bool>> cancelFlag_;
    QString error_;
};

// 彻底删除作业（递归删除）
class DeleteJob : public FileJob {
    Q_OBJECT
public:
    DeleteJob(const QList<QUrl> &sources, QObject *parent = nullptr);

protected:
    bool execute(QString *error) override;
    QStringList affectedDirectories() const override;

private:
    QList<QUrl> sources_;
};

// 移到回收站作业
class TrashJob : public FileJob {
    Q_OBJECT
public:
    TrashJob(const QList<QUrl> &sources, QObject *parent = nullptr);

protected:
    bool execute(QString *error) override;
    QStringList affectedDirectories() const override;

private:
    QList<QUrl> sources_;
};

} // namespace fm

#endif // FM_FILEOPS_FILE_JOB_H
