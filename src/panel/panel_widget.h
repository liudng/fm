#ifndef FM_PANEL_PANEL_WIDGET_H
#define FM_PANEL_PANEL_WIDGET_H

#include "../panel/panel_id.h"

#include <QWidget>

class QStackedWidget;

namespace fm {

class FileListView;
class FileListModel;
class FileListSortProxy;
class FileTabBar;
struct TabState;

// 单个面板：选项卡栏 + 文件列表视图栈
class PanelWidget : public QWidget {
    Q_OBJECT
public:
    PanelWidget(PanelId id, QWidget *parent = nullptr);

    PanelId id() const { return id_; }

    // 选项卡管理
    int addTab(const QString &path, int index = -1);     // 返回新选项卡索引
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

signals:
    void activeTabChanged(int index);
    void tabCountChanged();
    void pathChanged(const QString &path);
    void parentDirRequested();
    void openRequested(const QString &path);     // 进入子文件夹
    void contextMenuRequested(const QPoint &globalPos, bool hasSelection);

public slots:
    void navigateBack();
    void navigateForward();
    void navigateUp();
    void refresh();

private slots:
    void onTabChanged(int index);
    void onOpenRequested(const QModelIndex &proxyIndex);
    void onParentDirRequested();

private:
    struct TabData {
        FileListView *view = nullptr;
        FileListModel *model = nullptr;
        FileListSortProxy *proxy = nullptr;
        QList<QString> history;     // 导航历史
        int historyIndex = -1;      // 当前历史位置
    };

    void applyColumnConfig(FileListView *view);
    void navigateTo(const QString &path, bool addHistory);

    PanelId id_;
    FileTabBar *tabBar_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QList<TabData> tabs_;
};

} // namespace fm

#endif // FM_PANEL_PANEL_WIDGET_H
