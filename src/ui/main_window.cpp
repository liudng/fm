#include "main_window.h"

#include "favorites_menu_controller.h"
#include "volume_menu_controller.h"

#include "../core/column_manager.h"
#include "../core/config_manager.h"
#include "../core/favorite_manager.h"
#include "../core/session_state.h"
#include "../core/shortcut_manager.h"
#include "../dialogs/about_dialog.h"
#include "../dialogs/settings_dialog.h"
#include "../dialogs/settings_pages.h"
#include "../filelist/file_list_model.h"
#include "../panel/panel_container.h"
#include "../panel/panel_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleFactory>
#include <QTimer>
#include <QToolBar>

namespace fm {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Dual-Pane File Manager"));
    resize(1280, 800);

    // 工具栏
    toolbar_ = addToolBar(QStringLiteral("Toolbar"));
    toolbar_->setMovable(false);
    toolbar_->setIconSize(QSize(16, 16));

    // 双面板
    panelContainer_ = new PanelContainer(this);
    setCentralWidget(panelContainer_);

    buildMenuBar();
    restoreSession();

    // 默认布局：双面板各一个选项卡指向 Home（若 session 为空）
    if (panelContainer_->panel(PanelId::Panel1)->tabCount() == 0) {
        const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        panelContainer_->panel(PanelId::Panel1)->addTab(home, -1);
        panelContainer_->panel(PanelId::Panel2)->addTab(home, -1);
    }

    // 监听面板可见性/活动面板变化，更新菜单文字
    connect(panelContainer_, &PanelContainer::panelVisibilityChanged, this,
            &MainWindow::refreshPanelActions);
    connect(panelContainer_, &PanelContainer::activePanelChanged, this, [this]() {
        refreshPanelActions();
        updateToolbar();
    });
    connect(panelContainer_, &PanelContainer::orientationChanged, this,
            &MainWindow::refreshPanelActions);

    // 初始构建工具栏
    updateToolbar();

    // 监听配置变更（隐藏文件、面板可见性等）
    auto *cfg = ConfigManager::instance();
    connect(cfg, &ConfigManager::configChanged, this, [this](const QString &section) {
        if (section == QStringLiteral("Panels")) {
            applyPanelConfig();
        } else if (section == QStringLiteral("File_Browser")) {
            applyFileBrowserConfig();
        } else if (section == QStringLiteral("Favorites")) {
            // 收藏变化已在菜单 aboutToShow 时刷新
        }
    });

    // 应用初始配置（面板可见性、隐藏文件）
    applyPanelConfig();
    applyFileBrowserConfig();

    refreshPanelActions();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenuBar()
{
    buildFileMenu(menuBar()->addMenu(tr("&File")));
    buildFavoritesMenu(menuBar()->addMenu(tr("F&avorites")));
    buildSettingsMenu(menuBar()->addMenu(tr("&Settings")));
    buildHelpMenu(menuBar()->addMenu(tr("&Help")));
}

void MainWindow::buildFileMenu(QMenu *menu)
{
    // 卷段与外部设备段由 VolumeMenuController 托管：
    // - aboutToShow 时同步枚举已挂载卷 + 异步枚举外部设备
    // - 右键挂载/卸载/弹出
    // - 左键点击已挂载卷项通过 navigateRequested 信号导航
    volumeMenuController_ = new VolumeMenuController(menu, this);
    volumeMenuController_->setup();
    connect(volumeMenuController_, &VolumeMenuController::navigateRequested, this,
            [this](const QString &mountPoint) {
                // 在活动面板的活动选项卡中打开挂载点（不新建选项卡）
                auto *p = panelContainer_->activePanel();
                if (p) p->openPath(mountPoint);
            });
    connect(volumeMenuController_, &VolumeMenuController::statusMessageRequested, this,
            [this](const QString &msg, int timeout) { statusBar()->showMessage(msg, timeout); });
    connect(volumeMenuController_, &VolumeMenuController::operationFailed, this,
            [this](const QString &errorMsg) {
                QMessageBox::warning(this, tr("Volume Operation Failed"), errorMsg);
            });

    auto *exitAction = menu->addAction(tr("&Quit"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExit);
    exitAction->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));
    ShortcutManager::instance()->applyToAction(exitAction, QStringLiteral("file.quit"));
}

void MainWindow::buildFavoritesMenu(QMenu *menu)
{
    // 收藏菜单由 FavoritesMenuController 托管：
    // - 构建"添加到收藏..."项 + 分隔符 + 动态收藏列表
    // - aboutToShow 时从 FavoriteManager 刷新
    // - 右键删除收藏项
    favoritesMenuController_ = new FavoritesMenuController(menu, this);
    favoritesMenuController_->setup();
    connect(favoritesMenuController_, &FavoritesMenuController::addFavoriteRequested, this,
            &MainWindow::onAddFavorite);
    connect(favoritesMenuController_, &FavoritesMenuController::favoriteTriggered, this,
            &MainWindow::onFavoriteTriggered);
}

void MainWindow::buildSettingsMenu(QMenu *menu)
{
    // 语言子菜单
    languageMenu_ = menu->addMenu(tr("&Language"));
    languageGroup_ = new QActionGroup(languageMenu_);
    auto *enAction = languageMenu_->addAction(tr("&English"));
    enAction->setCheckable(true);
    enAction->setData(QStringLiteral("en"));
    auto *zhAction = languageMenu_->addAction(tr("&Chinese"));
    zhAction->setCheckable(true);
    zhAction->setData(QStringLiteral("zh"));
    languageGroup_->addAction(enAction);
    languageGroup_->addAction(zhAction);
    connect(languageGroup_, &QActionGroup::triggered, this, &MainWindow::onLanguageChanged);

    // 根据配置选中当前语言（而非硬编码英文）
    const QString curLang =
        ConfigManager::instance()
            ->value(QStringLiteral("UI"), QStringLiteral("language"), QStringLiteral("en"))
            .toString();
    bool langChecked = false;
    for (QAction *a : languageGroup_->actions()) {
        if (a->data().toString() == curLang) {
            a->setChecked(true);
            langChecked = true;
            break;
        }
    }
    if (!langChecked) enAction->setChecked(true);

    // 主题子菜单
    themeMenu_ = menu->addMenu(tr("&Theme"));
    themeGroup_ = new QActionGroup(themeMenu_);
    auto *defaultTheme = themeMenu_->addAction(tr("&Default"));
    defaultTheme->setCheckable(true);
    defaultTheme->setData(QStringLiteral(""));
    for (const QString &key : QStyleFactory::keys()) {
        auto *a = themeMenu_->addAction(key);
        a->setCheckable(true);
        a->setData(key);
        themeGroup_->addAction(a);
    }
    themeGroup_->addAction(defaultTheme);
    defaultTheme->setChecked(true);
    connect(themeGroup_, &QActionGroup::triggered, this, &MainWindow::onThemeChanged);

    menu->addSeparator();

    auto *toggleActive = menu->addAction(tr("&Switch Active Panel"), QKeySequence(Qt::Key_Tab),
                                         this, &MainWindow::onToggleActivePanel);
    toggleActive->setIcon(QIcon::fromTheme(QStringLiteral("go-jump")));
    ShortcutManager::instance()->applyToAction(toggleActive,
                                               QStringLiteral("settings.switch_active_panel"));

    toggleOrientationAction_ = menu->addAction(QString(), this, &MainWindow::onToggleOrientation);
    ShortcutManager::instance()->applyToAction(toggleOrientationAction_,
                                               QStringLiteral("settings.toggle_orientation"));

    auto *resetSplitterAction =
        menu->addAction(tr("&Reset Splitter"), this, &MainWindow::onResetSplitter);

    menu->addSeparator();

    togglePanel1Action_ = menu->addAction(QString(), this, &MainWindow::onTogglePanel1Visible);
    ShortcutManager::instance()->applyToAction(togglePanel1Action_,
                                               QStringLiteral("settings.toggle_panel1"));

    togglePanel2Action_ = menu->addAction(QString(), this, &MainWindow::onTogglePanel2Visible);
    ShortcutManager::instance()->applyToAction(togglePanel2Action_,
                                               QStringLiteral("settings.toggle_panel2"));

    menu->addSeparator();

    toggleToolbarAction_ = menu->addAction(tr("Show &Toolbar"), this,
                                            &MainWindow::onToggleToolbarVisible);
    toggleToolbarAction_->setCheckable(true);
    toggleToolbarAction_->setChecked(true);

    menu->addSeparator();

    toggleHiddenAction_ =
        menu->addAction(tr("Show &Hidden Files"), QKeySequence(Qt::CTRL | Qt::Key_H), this,
                        &MainWindow::onToggleHiddenFiles);
    toggleHiddenAction_->setCheckable(true);
    ShortcutManager::instance()->applyToAction(toggleHiddenAction_,
                                               QStringLiteral("settings.toggle_hidden"));

    menu->addSeparator();

    auto *settingsAction = menu->addAction(tr("&Settings..."), this, &MainWindow::onOpenSettings);
    settingsAction->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));
}

void MainWindow::buildHelpMenu(QMenu *menu)
{
    auto *aboutAction = menu->addAction(tr("&About"), this, &MainWindow::onAbout);
    aboutAction->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    ShortcutManager::instance()->applyToAction(aboutAction, QStringLiteral("help.about"));
}

void MainWindow::refreshPanelActions()
{
    if (!panelContainer_) return;
    if (toggleOrientationAction_) {
        const bool horizontal = panelContainer_->orientation() == Qt::Horizontal;
        toggleOrientationAction_->setText(horizontal ? tr("Switch to &Vertical Layout")
                                                     : tr("Switch to &Horizontal Layout"));
    }
    if (togglePanel1Action_) {
        const bool visible = panelContainer_->isPanelVisible(PanelId::Panel1);
        togglePanel1Action_->setText(visible ? tr("Hide &Panel 1") : tr("Show &Panel 1"));
    }
    if (togglePanel2Action_) {
        const bool visible = panelContainer_->isPanelVisible(PanelId::Panel2);
        togglePanel2Action_->setText(visible ? tr("Hide &Panel 2") : tr("Show &Panel 2"));
    }
    if (toggleHiddenAction_) {
        const bool showHidden =
            ConfigManager::instance()
                ->value(QStringLiteral("File_Browser"), QStringLiteral("showHidden"), false)
                .toBool();
        toggleHiddenAction_->setChecked(showHidden);
    }
}

void MainWindow::updateToolbar()
{
    if (!toolbar_ || !panelContainer_) return;
    toolbar_->clear();
    auto *panel = panelContainer_->activePanel();
    if (!panel) return;
    // 工具栏复用活动面板的持久化 QAction，状态随面板自动更新
    for (QAction *action : panel->toolbarActions()) {
        if (action) {
            toolbar_->addAction(action);
        } else {
            toolbar_->addSeparator();
        }
    }
}

void MainWindow::restoreSession()
{
    auto *cfg = ConfigManager::instance();
    // 读取 [Session]
    const QString sessionData =
        cfg->value(QStringLiteral("Session"), QStringLiteral("data")).toString();
    if (sessionData.isEmpty()) return;

    LayoutState state;
    if (!SessionState::deserialize(sessionData, state)) return;

    panelContainer_->setHorizontalSizes(state.horizontalSizes);
    panelContainer_->setVerticalSizes(state.verticalSizes);
    panelContainer_->setOrientation(state.orientation);
    panelContainer_->setPanelVisible(PanelId::Panel1, state.panelVisible[0]);
    panelContainer_->setPanelVisible(PanelId::Panel2, state.panelVisible[1]);
    // 恢复工具栏显示/隐藏状态
    if (toolbar_) toolbar_->setVisible(state.toolbarVisible);
    if (toggleToolbarAction_) toggleToolbarAction_->setChecked(state.toolbarVisible);
    // 应用当前方向的保存比例（若有）
    // 注意：在窗口 show() 之前调用 setSplitterSizes 通常不会生效
    // （splitter 尺寸为 0，比例计算无意义）。因此延后到事件循环中执行，
    // 此时窗口已 show，并避免被后续 applyPanelConfig 的 setPanelVisible 覆盖。
    const QList<int> curSizes =
        (state.orientation == Qt::Horizontal) ? state.horizontalSizes : state.verticalSizes;
    if (!curSizes.isEmpty()) {
        QTimer::singleShot(0, this,
                           [this, curSizes]() { panelContainer_->setSplitterSizes(curSizes); });
    }

    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        p->setTabStates(state.panels[i].tabs, state.panels[i].activeTabIndex);
    }
}

void MainWindow::onExit()
{
    close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 保存布局到 [Session]
    LayoutState state;
    state.orientation = panelContainer_->orientation();
    state.panelVisible[0] = panelContainer_->isPanelVisible(PanelId::Panel1);
    state.panelVisible[1] = panelContainer_->isPanelVisible(PanelId::Panel2);
    state.toolbarVisible = toolbar_ ? toolbar_->isVisible() : true;
    // 当前方向的实际比例同步到对应成员，再分别持久化左右/上下比例
    const Qt::Orientation curOri = panelContainer_->orientation();
    const QList<int> curSizes = panelContainer_->splitterSizes();
    if (curOri == Qt::Horizontal)
        panelContainer_->setHorizontalSizes(curSizes);
    else
        panelContainer_->setVerticalSizes(curSizes);
    state.horizontalSizes = panelContainer_->horizontalSizes();
    state.verticalSizes = panelContainer_->verticalSizes();

    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        state.panels[i].tabs = p->tabStates();
        state.panels[i].activeTabIndex = p->activeTabIndex();
    }

    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("Session"), QStringLiteral("data"),
                  SessionState::serialize(state));

    event->accept();
}

void MainWindow::addPathsToPanels(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    // path1 → 面板1，path2 → 面板2
    panelContainer_->panel(PanelId::Panel1)->addTab(paths.at(0), -1);
    if (paths.size() >= 2) {
        panelContainer_->panel(PanelId::Panel2)->addTab(paths.at(1), -1);
    }
    panelContainer_->setActivePanel(PanelId::Panel1);
}

// === 收藏菜单（布局采集与恢复）===

void MainWindow::onAddFavorite()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Add Favorite"), tr("Favorite name:"),
                                               QLineEdit::Normal, tr("New Favorite"), &ok);
    if (!ok || name.isEmpty()) return;

    // 构建当前布局状态
    LayoutState state;
    state.orientation = panelContainer_->orientation();
    state.panelVisible[0] = panelContainer_->isPanelVisible(PanelId::Panel1);
    state.panelVisible[1] = panelContainer_->isPanelVisible(PanelId::Panel2);
    state.toolbarVisible = toolbar_ ? toolbar_->isVisible() : true;
    // 当前方向的实际比例同步到对应成员，再分别持久化左右/上下比例
    {
        const Qt::Orientation ori = panelContainer_->orientation();
        const QList<int> sizes = panelContainer_->splitterSizes();
        if (ori == Qt::Horizontal)
            panelContainer_->setHorizontalSizes(sizes);
        else
            panelContainer_->setVerticalSizes(sizes);
    }
    state.horizontalSizes = panelContainer_->horizontalSizes();
    state.verticalSizes = panelContainer_->verticalSizes();
    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        state.panels[i].tabs = p->tabStates();
        state.panels[i].activeTabIndex = p->activeTabIndex();
    }

    if (!FavoriteManager::instance()->addFavorite(name, state)) {
        QMessageBox::warning(this, tr("Add Favorite"),
                             tr("A favorite with this name already exists."));
    }
}

void MainWindow::onFavoriteTriggered(const QString &name)
{
    LayoutState state;
    if (!FavoriteManager::instance()->loadFavorite(name, state)) return;

    panelContainer_->setHorizontalSizes(state.horizontalSizes);
    panelContainer_->setVerticalSizes(state.verticalSizes);
    panelContainer_->setOrientation(state.orientation);
    panelContainer_->setPanelVisible(PanelId::Panel1, state.panelVisible[0]);
    panelContainer_->setPanelVisible(PanelId::Panel2, state.panelVisible[1]);
    // 恢复工具栏显示/隐藏状态
    if (toolbar_) toolbar_->setVisible(state.toolbarVisible);
    if (toggleToolbarAction_) toggleToolbarAction_->setChecked(state.toolbarVisible);
    // 应用当前方向的保存比例（若有）
    const QList<int> &curSizes =
        (state.orientation == Qt::Horizontal) ? state.horizontalSizes : state.verticalSizes;
    if (!curSizes.isEmpty()) panelContainer_->setSplitterSizes(curSizes);
    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        p->setTabStates(state.panels[i].tabs, state.panels[i].activeTabIndex);
    }
}

// === 设置菜单 ===

void MainWindow::onToggleActivePanel()
{
    const PanelId cur = panelContainer_->activePanelId();
    panelContainer_->setActivePanel(cur == PanelId::Panel1 ? PanelId::Panel2 : PanelId::Panel1);
}

void MainWindow::onToggleOrientation()
{
    const bool horizontal = panelContainer_->orientation() == Qt::Horizontal;
    panelContainer_->setOrientation(horizontal ? Qt::Vertical : Qt::Horizontal);
    // 同步更新配置文件，使设置对话框中的布局选项与运行时状态一致
    ConfigManager::instance()->setValue(QStringLiteral("Panels"), QStringLiteral("orientation"),
                                        static_cast<int>(panelContainer_->orientation()));
}

void MainWindow::onResetSplitter()
{
    // 将两个面板的比例重置为各占窗口一半
    panelContainer_->setSplitterSizes({1, 1});
    // 同步更新当前方向的记忆比例，避免下次切换方向时恢复为旧值
    const Qt::Orientation curOri = panelContainer_->orientation();
    const QList<int> sizes = panelContainer_->splitterSizes();
    if (curOri == Qt::Horizontal)
        panelContainer_->setHorizontalSizes(sizes);
    else
        panelContainer_->setVerticalSizes(sizes);
}

void MainWindow::onTogglePanel1Visible()
{
    panelContainer_->setPanelVisible(PanelId::Panel1,
                                     !panelContainer_->isPanelVisible(PanelId::Panel1));
}

void MainWindow::onTogglePanel2Visible()
{
    panelContainer_->setPanelVisible(PanelId::Panel2,
                                     !panelContainer_->isPanelVisible(PanelId::Panel2));
}

void MainWindow::onToggleToolbarVisible()
{
    const bool visible = toggleToolbarAction_ ? toggleToolbarAction_->isChecked() : true;
    if (toolbar_) toolbar_->setVisible(visible);
}

void MainWindow::onToggleHiddenFiles()
{
    const bool show = toggleHiddenAction_ ? toggleHiddenAction_->isChecked() : false;
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("File_Browser"), QStringLiteral("showHidden"), show);
    // configChanged 信号会触发 applyFileBrowserConfig
}

void MainWindow::onLanguageChanged(QAction *action)
{
    if (!action) return;
    const QString lang = action->data().toString();
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("language"), lang);
    // 实际切换在 Phase 5 实现
    QMessageBox::information(this, tr("Language"), tr("Language will be applied after restart."));
}

void MainWindow::onThemeChanged(QAction *action)
{
    if (!action) return;
    const QString theme = action->data().toString();
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("theme"), theme);
    // FmApplication 会监听 configChanged 信号应用主题
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(this);
    // 添加四个设置页
    dlg.addPage(new UiSettingsPage(this));
    dlg.addPage(new PanelSettingsPage(this));
    dlg.addPage(new FileBrowserSettingsPage(this));
    dlg.addPage(new FileOperationsSettingsPage(this));
    dlg.addPage(new ShortcutSettingsPage(this));
    dlg.exec();
}

void MainWindow::applyPanelConfig()
{
    auto *cfg = ConfigManager::instance();
    const int orient = cfg->value(QStringLiteral("Panels"), QStringLiteral("orientation"),
                                  static_cast<int>(Qt::Horizontal))
                           .toInt();
    const bool p1Visible =
        cfg->value(QStringLiteral("Panels"), QStringLiteral("panel1Visible"), true).toBool();
    const bool p2Visible =
        cfg->value(QStringLiteral("Panels"), QStringLiteral("panel2Visible"), true).toBool();

    panelContainer_->setOrientation(static_cast<Qt::Orientation>(orient));
    panelContainer_->setPanelVisible(PanelId::Panel1, p1Visible);
    panelContainer_->setPanelVisible(PanelId::Panel2, p2Visible);
    // 活动面板由会话恢复，不在此处设置
}

void MainWindow::applyFileBrowserConfig()
{
    auto *cfg = ConfigManager::instance();
    const bool showHidden =
        cfg->value(QStringLiteral("File_Browser"), QStringLiteral("showHidden"), false).toBool();
    if (toggleHiddenAction_) {
        QSignalBlocker blocker(toggleHiddenAction_);
        toggleHiddenAction_->setChecked(showHidden);
    }

    // 应用到所有面板所有选项卡的 model
    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        if (!p) continue;
        auto *m = p->model();
        if (m) m->setShowHidden(showHidden);
    }
}

// === 帮助菜单 ===

void MainWindow::onAbout()
{
    AboutDialog dlg(this);
    dlg.exec();
}

} // namespace fm
