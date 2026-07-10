#ifndef FM_FILELIST_FILE_LIST_VIEW_H
#define FM_FILELIST_FILE_LIST_VIEW_H

#include <QTreeView>

namespace fm {

// 文件列表视图
// - 双击文件夹/文件触发 openRequested
// - 双击 ".." 触发 parentDirRequested
// - 右键菜单：通过 contextMenuRequested 信号
// - 键盘导航：Enter/Backspace/F2/F5/Ctrl+A 等
class FileListView : public QTreeView
{
    Q_OBJECT
public:
    explicit FileListView(QWidget *parent = nullptr);

    void setColumnConfig(const QList<int> &visibleColumns, const QMap<int, int> &columnWidths);

signals:
    void openRequested(const QModelIndex &proxyIndex);
    void parentDirRequested();
    void contextMenuRequested(const QPoint &globalPos);
    // 键盘导航信号
    void openKeyPressed();             // Enter
    void renameRequested();            // F2
    void refreshRequested();           // F5 / Ctrl+R
    void selectAllRequested();         // Ctrl+A
    void trashRequested();             // Delete
    void deletePermanentlyRequested(); // Shift+Delete
    void copyRequested();              // Ctrl+C
    void cutRequested();               // Ctrl+X
    void pasteRequested();             // Ctrl+V
    void copyPathRequested();          // Ctrl+Shift+C
    void copyFileNameRequested();      // Ctrl+Shift+N

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

} // namespace fm

#endif // FM_FILELIST_FILE_LIST_VIEW_H
