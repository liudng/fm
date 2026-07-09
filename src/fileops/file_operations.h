#ifndef FM_FILEOPS_FILE_OPERATIONS_H
#define FM_FILEOPS_FILE_OPERATIONS_H

#include <QPointer>
#include <QObject>
#include <QUrl>

namespace fm {

class ProgressDialog;
enum class ConflictResolution;

// 文件操作门面（单例）
// - 复制/移动/删除/回收站：异步（QtConcurrent）
// - 重命名/新建：同步（快速）
// - 冲突处理：在主线程弹对话框
class FileOperations : public QObject {
    Q_OBJECT
public:
    static FileOperations *instance();

    // 复制 sources 到 destDir
    void copy(const QList<QUrl> &sources, const QString &destDir);
    // 移动 sources 到 destDir
    void move(const QList<QUrl> &sources, const QString &destDir);
    // 移到回收站
    void trash(const QList<QUrl> &sources);
    // 彻底删除（弹二次确认）
    void deletePermanently(const QList<QUrl> &sources);
    // 从剪贴板粘贴到 destDir
    void pasteFromClipboard(const QString &destDir);
    // 重命名
    void rename(const QUrl &target, const QString &newName);
    // 新建文件
    void createFile(const QString &dir, const QString &defaultName);
    // 新建文件夹
    void createDir(const QString &dir, const QString &defaultName);
    // 用系统默认程序打开（先检查 [OpenWith] 是否已记住选择）
    void openWithDefault(const QUrl &file);
    // 用指定 .desktop 应用打开文件
    void openWithApplication(const QUrl &file, const QString &desktopFile);
    // 用自定义命令打开文件
    void openWithCommand(const QUrl &file, const QString &command);

signals:
    // 操作完成后刷新该目录
    void directoryChanged(const QString &dir);
    void operationCompleted();
    void operationFailed(const QString &errorMsg);

private:
    FileOperations(QObject *parent = nullptr);

    // 异步作业的辅助
    void runCopyMove(const QList<QUrl> &sources, const QString &destDir, bool isMove);
    void runTrash(const QList<QUrl> &sources);
    void runDelete(const QList<QUrl> &sources);

    // 冲突解决（主线程，阻塞工作线程）
    ConflictResolution resolveConflict(const QString &sourceName,
                                        const QString &destPath,
                                        bool allowBatch);

    // 生成不冲突的新名
    static QString uniqueName(const QString &dir, const QString &name);

    // 批量冲突记忆
    ConflictResolution batchResolution_;
    bool hasBatchResolution_ = false;

    QPointer<ProgressDialog> progressDialog_;
};

} // namespace fm

#endif // FM_FILEOPS_FILE_OPERATIONS_H
