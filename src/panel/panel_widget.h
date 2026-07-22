#ifndef FM_PANEL_PANEL_WIDGET_H
#define FM_PANEL_PANEL_WIDGET_H

#include "../panel/panel_id.h"

#include <QWidget>

class QStackedWidget;
class QMenu;
class QPoint;

namespace fm {

class FileListView;
class FileListModel;
class FileListSortProxy;
class FileTabBar;
class PanelContainer;
struct TabState;
struct FileItem;

// 单个面板：选项卡栏 + 文件列表视图栈
class PanelWidget : public QWidget
{
    Q_OBJECT
public:
    PanelWidget(PanelId id, QWidget *parent = nullptr);

    PanelId id() const { return id_; }

    // 选项卡管理
    int addTab(const QString &path, int index = -1); // 返回新选项卡索引
    void closeTab(int index);
    void closeOtherTabs(int index);
    int cloneTab(int index);
    int tabCount() const;
    int activeTabIndex() const;
    void setActiveTab(int index);
    QString tabPath(int index) const;
    QString activeTabPath() const;

    // 文件列表（活动选项卡）
    FileListView *listView() const;
    FileListModel *model() const;
    FileListSortProxy *proxyModel() const;
    FileTabBar *tabBar() const;

    // 会话状态
    QList<TabState> tabStates() const;
    void setTabStates(const QList<TabState> &states, int activeIndex);

    // 选中文件项
    QList<FileItem> selectedItems() const;
    QList<QUrl> selectedUrls() const;
    bool hasSelection() const;
    bool hasSingleSelection() const;

    // 显示右键菜单
    void showContextMenu(const QPoint &globalPos, bool hasSelection);

    // 持久化 QAction（用于工具栏与右键菜单共享）
    QList<QAction *> toolbarActions() const;
    void updateActionStates();

    // 活动面板状态
    void setActivePanel(bool active);

signals:
    void activeTabChanged(int index);
    void tabCountChanged();
    void pathChanged(const QString &path);
    void parentDirRequested();
    void openRequested(const QString &path); // 进入子文件夹
    void contextMenuRequested(const QPoint &globalPos, bool hasSelection);
    void selectionChanged(); // 选中项变化时发射

public slots:
    void navigateBack();
    void navigateForward();
    void navigateUp();
    void refresh();
    void openPath(const QString &path); // 在活动选项卡中打开路径

public slots:
    // 右键菜单/工具栏动作槽
    void onOpen();
    void onOpenWith();
    void onCopy();
    void onCut();
    void onPaste();
    void onCopyToOpposite();
    void onCutToOpposite();
    void onTrash();
    void onDeletePermanently();
    void onRename();
    void onProperties();
    void onCopyPath();
    void onCopyFileName();
    void onNewFile();
    void onNewFolder();

private slots:
    void onTabChanged(int index);
    void onOpenRequested(const QModelIndex &proxyIndex);
    void onParentDirRequested();

private:
    struct TabData
    {
        FileListView *view = nullptr;
        FileListModel *model = nullptr;
        FileListSortProxy *proxy = nullptr;
        QList<QString> history; // 导航历史
        int historyIndex = -1;  // 当前历史位置
    };

    void createActions();
    void applyColumnConfig(FileListView *view);
    void navigateTo(const QString &path, bool addHistory);
    void clearAllTabs();                   // 清空所有选项卡（包括最后一个，不受 closeTab 限制）
    PanelContainer *findContainer() const; // 向上遍历父链查找 PanelContainer
    QString oppositePanelPath() const;
    bool oppositePanelVisible() const;
    QString currentDir() const;

    PanelId id_;
    bool isActivePanel_ = false; // 是否为活动面板
    FileTabBar *tabBar_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QList<TabData> tabs_;

    // 持久化动作
    QAction *actBack_ = nullptr;
    QAction *actForward_ = nullptr;
    QAction *actUp_ = nullptr;
    QAction *actHome_ = nullptr;
    QAction *actNewFile_ = nullptr;
    QAction *actNewFolder_ = nullptr;
    QAction *actRefresh_ = nullptr;
    QAction *actOpen_ = nullptr;
    QAction *actOpenWith_ = nullptr;
    QAction *actRename_ = nullptr;
    QAction *actCut_ = nullptr;
    QAction *actCopy_ = nullptr;
    QAction *actPaste_ = nullptr;
    QAction *actCutToOpp_ = nullptr;
    QAction *actCopyToOpp_ = nullptr;
    QAction *actCopyPath_ = nullptr;
    QAction *actCopyName_ = nullptr;
    QAction *actTrash_ = nullptr;
    QAction *actDelete_ = nullptr;
    QAction *actProperties_ = nullptr;
    QAction *actNewTab_ = nullptr;
    QAction *actCloseTab_ = nullptr;
    QAction *actCloneTab_ = nullptr;
    QAction *actNextTab_ = nullptr;
    QAction *actPrevTab_ = nullptr;
};

} // namespace fm

#endif // FM_PANEL_PANEL_WIDGET_H
