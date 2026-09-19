#ifndef FM_UI_MAIN_WINDOW_H
#define FM_UI_MAIN_WINDOW_H

#include <QMainWindow>

class QAction;
class QActionGroup;
class QMenu;
class QToolBar;

namespace fm {

class PanelContainer;
class PanelWidget;
class VolumeMenuController;
class FavoritesMenuController;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    PanelContainer *panelContainer() const { return panelContainer_; }

    // 单实例接收到 path 列表时调用
    void addPathsToPanels(const QStringList &paths);

    // org.freedesktop.FileManager1 D-Bus 服务请求处理（均为本地路径）
    // ShowFolders：在活动面板打开各文件夹（每个文件夹一个选项卡）
    void openFolders(const QStringList &paths);
    // ShowItems：显示各项目所在文件夹并选中项目（按父目录分组，每组一个选项卡）
    void revealItems(const QStringList &paths);
    // ShowItemProperties：弹出各项目的属性对话框（非模态，可多个并存）
    void showItemProperties(const QStringList &paths);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onExit();

    // 收藏菜单（布局采集与恢复，由 FavoritesMenuController 信号触发）
    void onAddFavorite();
    void onFavoriteTriggered(const QString &name);

    // 设置菜单
    void onToggleActivePanel();
    void onToggleOrientation();
    void onResetSplitter();
    void onTogglePanel1Visible();
    void onTogglePanel2Visible();
    void onToggleToolbarVisible();
    void onToggleHiddenFiles();
    void onLanguageChanged(QAction *action);
    void onThemeChanged(QAction *action);
    void onOpenSettings();

    // 帮助菜单
    void onAbout();

private:
    void buildMenuBar();
    void buildFileMenu(QMenu *menu);
    void buildFavoritesMenu(QMenu *menu);
    void buildSettingsMenu(QMenu *menu);
    void buildHelpMenu(QMenu *menu);
    void refreshPanelActions();
    void updateToolbar();
    void restoreSession();
    void applyPanelConfig();       // 从 [Panels] 应用面板配置
    void applyFileBrowserConfig(); // 从 [File_Browser] 应用浏览器配置
    void bringToFront();           // 取消最小化并置前激活（D-Bus 外部唤起时）

    PanelContainer *panelContainer_ = nullptr;
    QToolBar *toolbar_ = nullptr;

    // 菜单控制器（托管卷段与收藏菜单的构建/刷新/右键交互）
    VolumeMenuController *volumeMenuController_ = nullptr;
    FavoritesMenuController *favoritesMenuController_ = nullptr;

    // 设置菜单中需要动态更新文字的项
    QAction *toggleOrientationAction_ = nullptr;
    QAction *togglePanel1Action_ = nullptr;
    QAction *togglePanel2Action_ = nullptr;
    QAction *toggleToolbarAction_ = nullptr;
    QAction *toggleHiddenAction_ = nullptr;
    QMenu *languageMenu_ = nullptr;
    QMenu *themeMenu_ = nullptr;
    QActionGroup *languageGroup_ = nullptr;
    QActionGroup *themeGroup_ = nullptr;
};

} // namespace fm

#endif // FM_UI_MAIN_WINDOW_H
