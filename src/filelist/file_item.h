#ifndef FM_FILELIST_FILE_ITEM_H
#define FM_FILELIST_FILE_ITEM_H

#include <QDateTime>
#include <QFile>
#include <QIcon>
#include <QString>

namespace fm {

// 文件列表中一项的数据
struct FileItem {
    QString name;              // 文件全名
    QString absolutePath;
    qint64 size = 0;          // 字节（文件夹为 0）
    qint64 diskUsage = 0;     // 实际占用磁盘空间（st_blocks * 512）
    bool isDir = false;
    bool isSymLink = false;
    QString symLinkTarget;     // 符号链接直接目标（不做多层解析）
    QString mimeTypeName;      // image/png
    QString mimeTypeComment;  // PNG 图像
    QIcon icon;
    QString owner;
    QString group;
    uint ownerId = 0;          // 所有者 UID（st_uid）
    uint groupId = 0;          // 所属组 GID（st_gid）
    QDateTime created;
    QDateTime modified;
    QDateTime accessed;        // 最后访问时间（st_atime）
    QDateTime statusChanged;   // 状态变更时间（st_ctime）
    QFile::Permissions permissions;
    quint64 inode = 0;
};

} // namespace fm

Q_DECLARE_METATYPE(fm::FileItem)

#endif // FM_FILELIST_FILE_ITEM_H
