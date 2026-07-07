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
    bool isDir = false;
    bool isSymLink = false;
    QString symLinkTarget;     // 符号链接直接目标（不做多层解析）
    QString mimeTypeName;      // image/png
    QString mimeTypeComment;  // PNG 图像
    QIcon icon;
    QString owner;
    QString group;
    QDateTime created;
    QDateTime modified;
    QFile::Permissions permissions;
    quint64 inode = 0;
};

} // namespace fm

Q_DECLARE_METATYPE(fm::FileItem)

#endif // FM_FILELIST_FILE_ITEM_H
