#include "file_item.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

#include <sys/stat.h>

namespace fm {

namespace {

// QMimeDatabase 内部带缓存，复用单实例
QMimeDatabase &mimeDb()
{
    static QMimeDatabase db;
    return db;
}

} // namespace

FileItem makeFileItem(const QFileInfo &fi)
{
    FileItem item;
    item.name = fi.fileName();
    item.absolutePath = fi.absoluteFilePath();
    item.size = fi.isDir() ? 0 : fi.size();
    item.isDir = fi.isDir();
    item.isSymLink = fi.isSymLink();
    item.symLinkTarget = fi.symLinkTarget();
    item.owner = fi.owner();
    item.group = fi.group();
    item.created = fi.birthTime();
    item.modified = fi.lastModified();
    item.accessed = fi.lastRead();
    item.permissions = fi.permissions();
    // inode、UID/GID、磁盘占用、状态变更时间 通过 stat() 获取
    struct stat st;
    if (::stat(fi.absoluteFilePath().toLocal8Bit().constData(), &st) == 0) {
        item.inode = st.st_ino;
        item.ownerId = st.st_uid;
        item.groupId = st.st_gid;
        item.diskUsage = static_cast<qint64>(st.st_blocks) * 512;
        item.statusChanged = QDateTime::fromSecsSinceEpoch(st.st_ctime);
    }

    // MIME 类型
    const QMimeType mime = mimeDb().mimeTypeForFile(fi);
    item.mimeTypeName = mime.name();
    item.mimeTypeComment = mime.comment();

    // 图标不在此填充：QFileIconProvider 依赖 QtWidgets，而本库（fm_core）不链接
    // QtWidgets；图标由文件列表模型（FileListModel）自行填充
    return item;
}

} // namespace fm
