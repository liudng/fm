#ifndef FM_FILELIST_FILE_ITEM_H
#define FM_FILELIST_FILE_ITEM_H

#include <QDateTime>
#include <QFile>
#include <QIcon>
#include <QString>

class QFileInfo;

namespace fm {

// 文件列表中一项的数据
struct FileItem
{
    QString name; // 文件全名
    QString absolutePath;
    qint64 size = 0;      // 字节（文件夹为 0）
    qint64 diskUsage = 0; // 实际占用磁盘空间（st_blocks * 512）
    bool isDir = false;
    bool isSymLink = false;
    QString symLinkTarget;   // 符号链接直接目标（不做多层解析）
    QString mimeTypeName;    // image/png
    QString mimeTypeComment; // PNG 图像
    QIcon icon;
    QString owner;
    QString group;
    uint ownerId = 0; // 所有者 UID（st_uid）
    uint groupId = 0; // 所属组 GID（st_gid）
    QDateTime created;
    QDateTime modified;
    QDateTime accessed;      // 最后访问时间（st_atime）
    QDateTime statusChanged; // 状态变更时间（st_ctime）
    QFile::Permissions permissions;
    quint64 inode = 0;
};

// 从 QFileInfo 构造完整 FileItem（含 stat 的 inode/UID/GID/磁盘占用、MIME 类型），
// 供文件列表模型与属性对话框共用；图标除外（QFileIconProvider 依赖 QtWidgets，
// 由 FileListModel 自行填充）
FileItem makeFileItem(const QFileInfo &fi);

} // namespace fm

Q_DECLARE_METATYPE(fm::FileItem)

#endif // FM_FILELIST_FILE_ITEM_H
