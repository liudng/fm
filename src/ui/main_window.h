#ifndef FM_UI_MAIN_WINDOW_H
#define FM_UI_MAIN_WINDOW_H

#include <QMainWindow>
#include <QPointer>

class QAction;
class QActionGroup;
class QMenu;
class QToolBar;

namespace fm {

class PanelContainer;
class PanelWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    PanelContainer *panelContainer() const { return panelContainer_; }

    // 单实例接收到 path 列表时调用
    void addPathsToPanels(const QStringList &paths);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onExit();

    // 收藏菜单
    void onAddFavorite();
    void onFavoriteTriggered(const QString &name);

    // 设置菜单
    void onToggleActivePanel();
    void onToggleOrientation();
    void onResetSplitter();
    void onTogglePanel1Visible();
    void onTogglePanel2Visible();
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
    void refreshFavoritesMenu();
    void refreshFileMenuVolumes();
    void refreshPanelActions();
    void updateToolbar();
    void restoreSession();
    void applyPanelConfig();          // 从 [Panels] 应用面板配置
    void applyFileBrowserConfig();    // 从 [File_Browser] 应用浏览器配置

    PanelContainer *panelContainer_ = nullptr;
    QToolBar *toolbar_ = nullptr;

    // 设置菜单中需要动态更新文字的项
    QAction *toggleOrientationAction_ = nullptr;
    QAction *togglePanel1Action_ = nullptr;
    QAction *togglePanel2Action_ = nullptr;
    QAction *toggleHiddenAction_ = nullptr;
    QMenu *favoritesMenu_ = nullptr;
    QMenu *languageMenu_ = nullptr;
    QMenu *themeMenu_ = nullptr;
    QMenu *fileMenu_ = nullptr;
    QAction *volSeparator_ = nullptr;     // 卷项与外部设备项之间的分隔符
    QAction *extSeparator_ = nullptr;     // 外部设备项与 Quit 之间的分隔符
    QList<QAction*> volActions_;          // 动态卷项（aboutToShow 时刷新）
    QList<QAction*> extActions_;          // 动态外部设备项（aboutToShow 时刷新）
    QActionGroup *languageGroup_ = nullptr;
    QActionGroup *themeGroup_ = nullptr;
};

} // namespace fm

#endif // FM_UI_MAIN_WINDOW_H
