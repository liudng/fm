#ifndef FM_FILEOPS_TRASH_CAN_H
#define FM_FILEOPS_TRASH_CAN_H

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QUrl>

namespace fm {

// FreeDesktop.org Trash 规范实现
// - ~/.local/share/Trash/{files,info}
// - 外部分区根目录 .Trash-1000/{files,info}
// Q_DECLARE_TR_FUNCTIONS 使非 QObject 类拥有独立翻译上下文
class TrashCan
{
    Q_DECLARE_TR_FUNCTIONS(fm::TrashCan)
public:
    // 将文件移到回收站
    // 成功返回 true，失败返回 false 并填充 errorMsg
    static bool moveToTrash(const QUrl &fileUrl, QString *errorMsg);

    // 批量移到回收站
    static bool moveToTrash(const QList<QUrl> &fileUrls, QString *errorMsg);

    // 获取文件对应的 Trash 目录（主目录或外部分区）
    static QString trashDirForFile(const QString &filePath);

private:
    // 准备 Trash 目录结构（不存在则创建）
    static bool ensureTrashDir(const QString &trashDir, QString *errorMsg);

    // 生成不冲突的目标文件名
    static QString uniqueTrashName(const QString &trashFilesDir, const QString &originalName);

    // 写 .trashinfo 文件
    static bool writeTrashInfo(const QString &infoPath, const QString &originalPath,
                               const QDateTime &deletionTime);
};

} // namespace fm

#endif // FM_FILEOPS_TRASH_CAN_H
