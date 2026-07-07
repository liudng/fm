#include "main_window.h"

#include "../core/config_manager.h"
#include "../core/session_state.h"
#include "../dialogs/about_dialog.h"
#include "../dialogs/input_name_dialog.h"
#include "../filelist/file_list_model.h"
#include "../fileops/file_operations.h"
#include "../panel/panel_container.h"
#include "../panel/panel_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QToolBar>

namespace fm {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("fm"));
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
    connect(panelContainer_, &PanelContainer::panelVisibilityChanged,
            this, &MainWindow::refreshPanelActions);
    connect(panelContainer_, &PanelContainer::activePanelChanged,
            this, &MainWindow::refreshPanelActions);
    connect(panelContainer_, &PanelContainer::orientationChanged,
            this, &MainWindow::refreshPanelActions);

    refreshPanelActions();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenuBar() {
    buildFileMenu(menuBar()->addMenu(tr("&File")));
    buildFavoritesMenu(menuBar()->addMenu(tr("F&avorites")));
    buildSettingsMenu(menuBar()->addMenu(tr("&Settings")));
    buildHelpMenu(menuBar()->addMenu(tr("&Help")));
}

void MainWindow::buildFileMenu(QMenu *menu) {
    // 卷列表（Phase 4 占位）
    // 此处添加占位项，后续阶段补全
    auto *volPlaceholder = menu->addAction(tr("(Volumes)"));
    volPlaceholder->setEnabled(false);

    menu->addSeparator();

    auto *newTabAction = menu->addAction(tr("New &Tab"), QKeySequence(Qt::CTRL | Qt::Key_T),
                                          this, &MainWindow::onNewTab);
    newTabAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));

    auto *closeTabAction = menu->addAction(tr("&Close Tab"), QKeySequence(Qt::CTRL | Qt::Key_W),
                                            this, &MainWindow::onCloseTab);
    closeTabAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));

    auto *cloneTabAction = menu->addAction(tr("&Clone Tab"),
                                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T),
                                            this, &MainWindow::onCloneTab);
    cloneTabAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));

    menu->addSeparator();

    auto *newFileAction = menu->addAction(tr("New &File"), this, &MainWindow::onNewFile);
    newFileAction->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));

    auto *newFolderAction = menu->addAction(tr("New &Folder"), QKeySequence(Qt::Key_F7),
                                             this, &MainWindow::onNewFolder);
    newFolderAction->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));

    menu->addSeparator();

    auto *exitAction = menu->addAction(tr("&Quit"), QKeySequence::Quit,
                                        this, &MainWindow::onExit);
    exitAction->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));
}

void MainWindow::buildFavoritesMenu(QMenu *menu) {
    favoritesMenu_ = menu;

    auto *addAction = menu->addAction(tr("&Add to Favorites..."), this, &MainWindow::onAddFavorite);
    addAction->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));

    menu->addSeparator();

    // 占位项，refreshFavoritesMenu 会替换
    auto *placeholder = menu->addAction(tr("(No favorites)"));
    placeholder->setEnabled(false);

    // 菜单显示前刷新
    connect(menu, &QMenu::aboutToShow, this, &MainWindow::refreshFavoritesMenu);
}

void MainWindow::buildSettingsMenu(QMenu *menu) {
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

    // 默认选中英文
    enAction->setChecked(true);

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

    toggleOrientationAction_ = menu->addAction(QString(), this,
                                                  &MainWindow::onToggleOrientation);

    menu->addSeparator();

    togglePanel1Action_ = menu->addAction(QString(), this,
                                            &MainWindow::onTogglePanel1Visible);
    togglePanel2Action_ = menu->addAction(QString(), this,
                                            &MainWindow::onTogglePanel2Visible);

    menu->addSeparator();

    toggleHiddenAction_ = menu->addAction(tr("Show &Hidden Files"),
                                            QKeySequence(Qt::CTRL | Qt::Key_H),
                                            this, &MainWindow::onToggleHiddenFiles);
    toggleHiddenAction_->setCheckable(true);

    menu->addSeparator();

    auto *settingsAction = menu->addAction(tr("&Settings..."), this,
                                            &MainWindow::onOpenSettings);
    settingsAction->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    settingsAction->setEnabled(false);  // Phase 3 实现
}

void MainWindow::buildHelpMenu(QMenu *menu) {
    auto *aboutAction = menu->addAction(tr("&About"), this, &MainWindow::onAbout);
    aboutAction->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
}

void MainWindow::refreshFavoritesMenu() {
    if (!favoritesMenu_) return;
    // 清空菜单（保留前两项 + 分隔符）
    const auto actions = favoritesMenu_->actions();
    // 找到分隔符位置
    int sepIndex = -1;
    for (int i = 0; i < actions.size(); ++i) {
        if (actions.at(i)->isSeparator()) {
            sepIndex = i;
            break;
        }
    }
    // 删除分隔符之后的所有项
    if (sepIndex >= 0) {
        for (int i = actions.size() - 1; i > sepIndex; --i) {
            favoritesMenu_->removeAction(actions.at(i));
        }
    }

    // 从配置读取收藏列表
    auto *cfg = ConfigManager::instance();
    const QStringList groups = cfg->value(QStringLiteral("Favorites"),
                                            QStringLiteral("groups")).toStringList();

    if (groups.isEmpty()) {
        auto *placeholder = favoritesMenu_->addAction(tr("(No favorites)"));
        placeholder->setEnabled(false);
        return;
    }

    for (const QString &name : groups) {
        auto *action = favoritesMenu_->addAction(name);
        connect(action, &QAction::triggered, this, [this, name]() {
            onFavoriteTriggered(name);
        });
    }
}

void MainWindow::refreshPanelActions() {
    if (!panelContainer_) return;
    if (toggleOrientationAction_) {
        const bool horizontal = panelContainer_->orientation() == Qt::Horizontal;
        toggleOrientationAction_->setText(
            horizontal ? tr("Switch to &Vertical Layout") : tr("Switch to &Horizontal Layout"));
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
        auto *p = panelContainer_->activePanel();
        if (p && p->model()) {
            // 假设所有面板共享同一 showHidden 设置；Phase 3 完善
            toggleHiddenAction_->setChecked(false);  // 后续从配置读取
        }
    }
}

void MainWindow::restoreSession() {
    auto *cfg = ConfigManager::instance();
    // 读取 [Session]
    const QString sessionData = cfg->value(QStringLiteral("Session"),
                                            QStringLiteral("data")).toString();
    if (sessionData.isEmpty()) return;

    LayoutState state;
    if (!SessionState::deserialize(sessionData, state)) return;

    panelContainer_->setOrientation(state.orientation);
    panelContainer_->setPanelVisible(PanelId::Panel1, state.panelVisible[0]);
    panelContainer_->setPanelVisible(PanelId::Panel2, state.panelVisible[1]);
    if (!state.splitterSizes.isEmpty())
        panelContainer_->setSplitterSizes(state.splitterSizes);

    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        p->setTabStates(state.panels[i].tabs, state.panels[i].activeTabIndex);
    }
    panelContainer_->setActivePanel(static_cast<PanelId>(state.activePanelIndex));
}

void MainWindow::onExit() {
    close();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // 保存布局到 [Session]
    LayoutState state;
    state.orientation = panelContainer_->orientation();
    state.activePanelIndex = static_cast<int>(panelContainer_->activePanelId());
    state.panelVisible[0] = panelContainer_->isPanelVisible(PanelId::Panel1);
    state.panelVisible[1] = panelContainer_->isPanelVisible(PanelId::Panel2);
    state.splitterSizes = panelContainer_->splitterSizes();

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

void MainWindow::addPathsToPanels(const QStringList &paths) {
    if (paths.isEmpty()) return;
    // path1 → 面板1，path2 → 面板2
    panelContainer_->panel(PanelId::Panel1)->addTab(paths.at(0), -1);
    if (paths.size() >= 2) {
        panelContainer_->panel(PanelId::Panel2)->addTab(paths.at(1), -1);
    }
    panelContainer_->setActivePanel(PanelId::Panel1);
}

// === 文件菜单 ===

void MainWindow::onNewTab() {
    auto *p = panelContainer_->activePanel();
    if (p) p->addTab(p->activeTabPath(), -1);
}

void MainWindow::onCloseTab() {
    auto *p = panelContainer_->activePanel();
    if (p) p->closeTab(p->activeTabIndex());
}

void MainWindow::onCloneTab() {
    auto *p = panelContainer_->activePanel();
    if (p) p->cloneTab(p->activeTabIndex());
}

void MainWindow::onNewFile() {
    auto *p = panelContainer_->activePanel();
    if (!p) return;
    const QString dir = p->activeTabPath();
    if (dir.isEmpty()) return;
    InputNameDialog dlg(tr("New File"), tr("File name:"), tr("New File"), this);
    QStringList existing = QDir(dir).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    dlg.setExistingNames(existing);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = dlg.name();
    if (name.isEmpty()) return;
    FileOperations::instance()->createFile(dir, name);
}

void MainWindow::onNewFolder() {
    auto *p = panelContainer_->activePanel();
    if (!p) return;
    const QString dir = p->activeTabPath();
    if (dir.isEmpty()) return;
    InputNameDialog dlg(tr("New Folder"), tr("Folder name:"), tr("New Folder"), this);
    QStringList existing = QDir(dir).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    dlg.setExistingNames(existing);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = dlg.name();
    if (name.isEmpty()) return;
    FileOperations::instance()->createDir(dir, name);
}

// === 收藏菜单 ===

void MainWindow::onAddFavorite() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Add Favorite"),
                                                tr("Favorite name:"), QLineEdit::Normal,
                                                tr("New Favorite"), &ok);
    if (!ok || name.isEmpty()) return;

    auto *cfg = ConfigManager::instance();
    QStringList groups = cfg->value(QStringLiteral("Favorites"),
                                    QStringLiteral("groups")).toStringList();
    if (groups.contains(name)) {
        QMessageBox::warning(this, tr("Add Favorite"),
                             tr("A favorite with this name already exists."));
        return;
    }
    groups.append(name);
    cfg->setValue(QStringLiteral("Favorites"), QStringLiteral("groups"), groups);

    // 保存当前布局到 [Favorites/<name>]
    LayoutState state;
    state.orientation = panelContainer_->orientation();
    state.activePanelIndex = static_cast<int>(panelContainer_->activePanelId());
    state.panelVisible[0] = panelContainer_->isPanelVisible(PanelId::Panel1);
    state.panelVisible[1] = panelContainer_->isPanelVisible(PanelId::Panel2);
    state.splitterSizes = panelContainer_->splitterSizes();
    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        state.panels[i].tabs = p->tabStates();
        state.panels[i].activeTabIndex = p->activeTabIndex();
    }
    cfg->setValue(QStringLiteral("Favorites/") + name, QStringLiteral("data"),
                  SessionState::serialize(state));
}

void MainWindow::onFavoriteTriggered(const QString &name) {
    auto *cfg = ConfigManager::instance();
    const QString data = cfg->value(QStringLiteral("Favorites/") + name,
                                    QStringLiteral("data")).toString();
    if (data.isEmpty()) return;
    LayoutState state;
    if (!SessionState::deserialize(data, state)) return;

    panelContainer_->setOrientation(state.orientation);
    panelContainer_->setPanelVisible(PanelId::Panel1, state.panelVisible[0]);
    panelContainer_->setPanelVisible(PanelId::Panel2, state.panelVisible[1]);
    if (!state.splitterSizes.isEmpty())
        panelContainer_->setSplitterSizes(state.splitterSizes);
    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        p->setTabStates(state.panels[i].tabs, state.panels[i].activeTabIndex);
    }
    panelContainer_->setActivePanel(static_cast<PanelId>(state.activePanelIndex));
}

// === 设置菜单 ===

void MainWindow::onToggleActivePanel() {
    const PanelId cur = panelContainer_->activePanelId();
    panelContainer_->setActivePanel(cur == PanelId::Panel1 ? PanelId::Panel2 : PanelId::Panel1);
}

void MainWindow::onToggleOrientation() {
    const bool horizontal = panelContainer_->orientation() == Qt::Horizontal;
    panelContainer_->setOrientation(horizontal ? Qt::Vertical : Qt::Horizontal);
}

void MainWindow::onTogglePanel1Visible() {
    panelContainer_->setPanelVisible(PanelId::Panel1,
                                      !panelContainer_->isPanelVisible(PanelId::Panel1));
}

void MainWindow::onTogglePanel2Visible() {
    panelContainer_->setPanelVisible(PanelId::Panel2,
                                      !panelContainer_->isPanelVisible(PanelId::Panel2));
}

void MainWindow::onToggleHiddenFiles() {
    const bool show = toggleHiddenAction_ ? toggleHiddenAction_->isChecked() : false;
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("File_Browser"), QStringLiteral("show_hidden"), show);
    // 应用到所有面板的所有选项卡
    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        if (!p) continue;
        // 简单刷新：通过 model()->setShowHidden（每个 tab）
        // PanelWidget 没有暴露 setShowHidden，先用 model() 设置活动 tab
        auto *m = p->model();
        if (m) m->setShowHidden(show);
    }
}

void MainWindow::onLanguageChanged(QAction *action) {
    if (!action) return;
    const QString lang = action->data().toString();
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("language"), lang);
    // 实际切换在 Phase 5 实现
    QMessageBox::information(this, tr("Language"),
                             tr("Language will be applied after restart."));
}

void MainWindow::onThemeChanged(QAction *action) {
    if (!action) return;
    const QString theme = action->data().toString();
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("theme"), theme);
    if (theme.isEmpty()) {
        QApplication::setStyle(QString());
    } else {
        QApplication::setStyle(QStyleFactory::create(theme));
    }
}

void MainWindow::onOpenSettings() {
    // Phase 3 实现设置对话框
}

// === 帮助菜单 ===

void MainWindow::onAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

} // namespace fm
