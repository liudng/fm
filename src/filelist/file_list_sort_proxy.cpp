#include "file_list_sort_proxy.h"

#include "../filelist/file_list_model.h"

#include <QModelIndex>

namespace fm {

FileListSortProxy::FileListSortProxy(QObject *parent) : QSortFilterProxyModel(parent) {}

void FileListSortProxy::sort(int column, Qt::SortOrder order)
{
    // 仅在主键变更时更新"上次主键"为次键
    // 注意：sort 会被反复调用，仅在列变化时才推进
    if (column != lastSortColumn_) {
        secondaryColumn_ = lastSortColumn_;
        secondaryOrder_ = lastSortOrder_;
    }
    lastSortColumn_ = column;
    lastSortOrder_ = order;
    QSortFilterProxyModel::sort(column, order);
}

QModelIndex FileListSortProxy::parentRowIndex() const
{
    if (!sourceModel()) return {};
    auto *model = qobject_cast<FileListModel *>(sourceModel());
    if (!model) return {};
    return mapFromSource(model->index(0, 0));
}

bool FileListSortProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    auto *model = qobject_cast<FileListModel *>(sourceModel());
    if (model) {
        // ".." 行始终排最前，无论升降序
        // 注意：Qt 在降序排序时，会对调 lessThan 的两个参数调用
        // （即调用 lessThan(b, a) 而非 lessThan(a, b)），因此需要
        // 根据当前排序方向返回相反的值，否则降序时 ".." 会排到最后。
        const bool leftParent = model->isParentRow(left);
        const bool rightParent = model->isParentRow(right);
        if (leftParent && rightParent) return false;
        if (leftParent) return lastSortOrder_ == Qt::AscendingOrder;
        if (rightParent) return lastSortOrder_ == Qt::DescendingOrder;

        // 文件大小列：按真实字节数比较，而非格式化字符串
        if (left.column() == FileListModel::ColSize) {
            const FileItem &li = model->itemAt(left);
            const FileItem &ri = model->itemAt(right);
            // 目录大小为 0，排在文件之后（递增时）
            const qint64 ls = li.isDir ? -1 : li.size;
            const qint64 rs = ri.isDir ? -1 : ri.size;
            if (ls != rs) return ls < rs;
            // 大小相同，回退到名称比较
        }

        // 实际占用磁盘空间：按真实字节数比较
        if (left.column() == FileListModel::ColDiskUsage) {
            const FileItem &li = model->itemAt(left);
            const FileItem &ri = model->itemAt(right);
            if (li.diskUsage != ri.diskUsage) return li.diskUsage < ri.diskUsage;
        }

        // UID/GID：按数值比较，而非字符串
        if (left.column() == FileListModel::ColOwnerUid) {
            const FileItem &li = model->itemAt(left);
            const FileItem &ri = model->itemAt(right);
            if (li.ownerId != ri.ownerId) return li.ownerId < ri.ownerId;
        }
        if (left.column() == FileListModel::ColGroupGid) {
            const FileItem &li = model->itemAt(left);
            const FileItem &ri = model->itemAt(right);
            if (li.groupId != ri.groupId) return li.groupId < ri.groupId;
        }

        // 日期列：按 QDateTime 比较，避免格式化字符串排序不一致
        if (left.column() == FileListModel::ColAccessed) {
            const FileItem &li = model->itemAt(left);
            const FileItem &ri = model->itemAt(right);
            if (li.accessed != ri.accessed) return li.accessed < ri.accessed;
        }
        if (left.column() == FileListModel::ColStatusChanged) {
            const FileItem &li = model->itemAt(left);
            const FileItem &ri = model->itemAt(right);
            if (li.statusChanged != ri.statusChanged) return li.statusChanged < ri.statusChanged;
        }
    }

    // 主键比较
    if (QSortFilterProxyModel::lessThan(left, right)) return true;
    if (QSortFilterProxyModel::lessThan(right, left)) return false;

    // 主键相等：用次键比较（若有效）
    if (secondaryColumn_ >= 0 && secondaryColumn_ != lastSortColumn_) {
        const QModelIndex l = left.sibling(left.row(), secondaryColumn_);
        const QModelIndex r = right.sibling(right.row(), secondaryColumn_);
        if (l.isValid() && r.isValid()) {
            const bool lt = QSortFilterProxyModel::lessThan(l, r);
            return (secondaryOrder_ == Qt::AscendingOrder) ? lt : !lt;
        }
    }
    return false;
}

} // namespace fm
