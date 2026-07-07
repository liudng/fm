#ifndef FM_FILELIST_FILE_LIST_VIEW_H
#define FM_FILELIST_FILE_LIST_VIEW_H

#include <QTreeView>

namespace fm {

// 文件列表视图
// - 双击文件夹/文件触发 openRequested
// - 双击 ".." 触发 parentDirRequested
// - 右键菜单：通过 contextMenuRequested 信号
class FileListView : public QTreeView {
    Q_OBJECT
public:
    explicit FileListView(QWidget *parent = nullptr);

    void setColumnConfig(const QList<int> &visibleColumns, const QMap<int, double> &widthRatios);

signals:
    void openRequested(const QModelIndex &proxyIndex);
    void parentDirRequested();
    void contextMenuRequested(const QPoint &globalPos);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};

} // namespace fm

#endif // FM_FILELIST_FILE_LIST_VIEW_H
