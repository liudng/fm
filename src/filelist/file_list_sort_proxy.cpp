#include "file_list_sort_proxy.h"

#include "../filelist/file_list_model.h"

#include <QModelIndex>

namespace fm {

FileListSortProxy::FileListSortProxy(QObject *parent)
    : QSortFilterProxyModel(parent) {}

void FileListSortProxy::sort(int column, Qt::SortOrder order) {
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

QModelIndex FileListSortProxy::parentRowIndex() const {
    if (!sourceModel()) return {};
    auto *model = qobject_cast<FileListModel*>(sourceModel());
    if (!model) return {};
    return mapFromSource(model->index(0, 0));
}

bool FileListSortProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    auto *model = qobject_cast<FileListModel*>(sourceModel());
    if (model) {
        // ".." 行始终排最前
        const bool leftParent = model->isParentRow(left);
        const bool rightParent = model->isParentRow(right);
        if (leftParent && rightParent) return false;
        if (leftParent) return true;
        if (rightParent) return false;
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
