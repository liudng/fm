#include "file_list_model.h"

#include <QDateTime>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUrl>

#include <sys/stat.h>

namespace fm {

namespace {

// 简单文件图标提供器
QFileIconProvider &iconProvider() {
    static QFileIconProvider provider;
    return provider;
}

QMimeDatabase &mimeDb() {
    static QMimeDatabase db;
    return db;
}

// 权限转 rwx 字符串
QString permissionsToString(QFile::Permissions perms) {
    QString s = QStringLiteral("----------");
    auto set = [&perms, &s](int idx, QFile::Permission p) {
        if (perms & p) s[idx] = QChar::fromLatin1('r' + idx % 3);
    };
    // owner
    if (perms & QFile::ReadOwner)  s[0] = 'r';
    if (perms & QFile::WriteOwner) s[1] = 'w';
    if (perms & QFile::ExeOwner)   s[2] = 'x';
    // group
    if (perms & QFile::ReadGroup)  s[3] = 'r';
    if (perms & QFile::WriteGroup) s[4] = 'w';
    if (perms & QFile::ExeGroup)   s[5] = 'x';
    // others
    if (perms & QFile::ReadOther)  s[6] = 'r';
    if (perms & QFile::WriteOther) s[7] = 'w';
    if (perms & QFile::ExeOther)   s[8] = 'x';
    return s;
}

} // namespace

FileListModel::FileListModel(QObject *parent)
    : QAbstractItemModel(parent) {}

bool FileListModel::hasParentRow() const {
    return hasParent_;
}

int FileListModel::parentRowOffset() const {
    return hasParent_ ? 1 : 0;
}

void FileListModel::setPath(const QString &path) {
    if (path_ == path) {
        reload();
        return;
    }
    beginResetModel();
    path_ = path;
    items_.clear();
    loadDirectory();
    endResetModel();
    emit pathChanged(path_);
}

void FileListModel::reload() {
    beginResetModel();
    items_.clear();
    loadDirectory();
    endResetModel();
}

void FileListModel::setShowHidden(bool show) {
    if (showHidden_ == show) return;
    showHidden_ = show;
    reload();
}

void FileListModel::setDateTimeFormat(const QString &format) {
    if (dateTimeFormat_ == format) return;
    dateTimeFormat_ = format;
    // 通知所有日期列的数据已变化（含 Created/Modified/Accessed/StatusChanged）
    if (!items_.isEmpty()) {
        const int lastRow = rowCount({}) - 1;
        emit dataChanged(index(0, ColCreated), index(lastRow, ColStatusChanged));
    }
}

void FileListModel::loadDirectory() {
    items_.clear();
    lastError_.clear();

    if (path_.isEmpty()) return;

    QFileInfo pathInfo(path_);
    if (!pathInfo.exists()) {
        lastError_ = tr("Path does not exist: %1").arg(path_);
        emit loadError(lastError_);
        return;
    }
    if (!pathInfo.isDir()) {
        lastError_ = tr("Not a directory: %1").arg(path_);
        emit loadError(lastError_);
        return;
    }
    if (!pathInfo.isReadable()) {
        lastError_ = tr("Permission denied: %1").arg(path_);
        emit loadError(lastError_);
        return;
    }

    // ".." 行：根目录无父目录
    QDir dir(path_);
    const QString root = dir.rootPath();
    hasParent_ = (QDir::cleanPath(path_) != root);

    QDir::Filters filters = QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot;
    if (showHidden_) filters |= QDir::Hidden;

    const QFileInfoList entries = dir.entryInfoList(filters, QDir::Unsorted);
    items_.reserve(entries.size());

    for (const QFileInfo &fi : entries) {
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
        QMimeType mime = mimeDb().mimeTypeForFile(fi);
        item.mimeTypeName = mime.name();
        item.mimeTypeComment = mime.comment();

        // 图标
        item.icon = iconProvider().icon(fi);

        items_.append(std::move(item));
    }
}

FileItem FileListModel::itemAt(const QModelIndex &index) const {
    if (!index.isValid()) return FileItem{};
    const int row = index.row();
    if (row < 0 || row >= rowCount({})) return FileItem{};
    if (row == 0 && hasParentRow()) return FileItem{};  // ".." 行无 FileItem
    return items_.at(row - parentRowOffset());
}

bool FileListModel::isParentRow(const QModelIndex &index) const {
    return index.isValid() && index.row() == 0 && hasParentRow();
}

QModelIndex FileListModel::index(int row, int column, const QModelIndex &parent) const {
    if (parent.isValid() || row < 0 || row >= rowCount({}) || column < 0 || column >= ColCount)
        return {};
    return createIndex(row, column);
}

QModelIndex FileListModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child)
    return {};
}

int FileListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return items_.size() + parentRowOffset();
}

int FileListModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return ColCount;
}

QVariant FileListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return {};
    const int row = index.row();
    const int col = index.column();

    // ".." 行
    if (row == 0 && hasParentRow()) {
        if (role == IsParentRowRole) return true;
        if (role == Qt::DisplayRole) {
            if (col == ColName) return QStringLiteral("..");
            return {};
        }
        if (role == Qt::DecorationRole && col == ColIcon) {
            return iconProvider().icon(QFileIconProvider::Folder);
        }
        if (role == FileItemRole) return {};
        if (role == IsParentRowRole) return true;
        return {};
    }

    if (row < parentRowOffset() || row >= rowCount({})) return {};
    const FileItem &item = items_.at(row - parentRowOffset());

    if (role == IsParentRowRole) return false;

    if (role == FileItemRole) {
        // 返回 FileItem（通过 QVariant 包装）
        return QVariant::fromValue(item);
    }

    if (role == Qt::DecorationRole && col == ColIcon) {
        return item.icon;
    }

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        switch (col) {
            case ColIcon:        return {};
            case ColName:        return item.name;
            case ColSize:        return item.isDir ? QString() : QLocale().formattedDataSize(item.size);
            case ColType:        return item.mimeTypeComment;
            case ColMimeType:    return item.mimeTypeName;
            case ColGroup:       return item.group;
            case ColOwner:       return item.owner;
            case ColOwnerUid:    return QString::number(item.ownerId);
            case ColGroupGid:    return QString::number(item.groupId);
            case ColCreated:     return dateTimeFormat_.isEmpty()
                                    ? item.created.toString(Qt::ISODate)
                                    : item.created.toString(dateTimeFormat_);
            case ColModified:    return dateTimeFormat_.isEmpty()
                                    ? item.modified.toString(Qt::ISODate)
                                    : item.modified.toString(dateTimeFormat_);
            case ColAccessed:    return dateTimeFormat_.isEmpty()
                                    ? item.accessed.toString(Qt::ISODate)
                                    : item.accessed.toString(dateTimeFormat_);
            case ColDiskUsage:   return item.isDir ? QString()
                                    : QLocale().formattedDataSize(item.diskUsage);
            case ColStatusChanged: return dateTimeFormat_.isEmpty()
                                    ? item.statusChanged.toString(Qt::ISODate)
                                    : item.statusChanged.toString(dateTimeFormat_);
            case ColPermissions: return permissionsToString(item.permissions);
        }
    }

    if (role == Qt::TextAlignmentRole) {
        if (col == ColSize || col == ColDiskUsage || col == ColOwnerUid || col == ColGroupGid)
            return Qt::AlignRight;
    }

    return {};
}

QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case ColIcon:        return tr("Icon");
        case ColName:        return tr("Name");
        case ColSize:        return tr("Size");
        case ColType:        return tr("Type");
        case ColMimeType:    return tr("MIME");
        case ColGroup:       return tr("Group");
        case ColOwner:       return tr("Owner");
        case ColOwnerUid:    return tr("UID");
        case ColGroupGid:    return tr("GID");
        case ColCreated:     return tr("Created");
        case ColModified:    return tr("Modified");
        case ColAccessed:    return tr("Accessed");
        case ColDiskUsage:   return tr("Disk Usage");
        case ColStatusChanged: return tr("Status Changed");
        case ColPermissions: return tr("Permissions");
    }
    return {};
}

Qt::ItemFlags FileListModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    if (isParentRow(index)) {
        // ".." 行不可选中（双击仍可触发 parentDirRequested）
        return Qt::ItemIsEnabled;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

} // namespace fm
