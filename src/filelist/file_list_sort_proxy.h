#ifndef FM_FILELIST_FILE_LIST_SORT_PROXY_H
#define FM_FILELIST_FILE_LIST_SORT_PROXY_H

#include <QSortFilterProxyModel>

namespace fm {

// 排序代理模型
// - ".." 行固定排在最前，不参与排序
// - 支持次要排序：保留上次排序列作为次要键
class FileListSortProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit FileListSortProxy(QObject *parent = nullptr);

    // 覆盖排序：处理 ".." 行与次要排序
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // 返回 ".." 行的索引（无则返回无效）
    QModelIndex parentRowIndex() const;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    int lastSortColumn_ = -1;
    Qt::SortOrder lastSortOrder_ = Qt::AscendingOrder;
    int secondaryColumn_ = -1;
    Qt::SortOrder secondaryOrder_ = Qt::AscendingOrder;
};

} // namespace fm

#endif // FM_FILELIST_FILE_LIST_SORT_PROXY_H
