#include "main_window.h"

#include "../core/column_manager.h"
#include "../core/config_manager.h"
#include "../core/favorite_manager.h"
#include "../core/session_state.h"
#include "../core/shortcut_manager.h"
#include "../core/volume_manager.h"
#include "../dialogs/about_dialog.h"
#include "../dialogs/input_name_dialog.h"
#include "../dialogs/settings_dialog.h"
#include "../dialogs/settings_pages.h"
#include "../filelist/file_list_model.h"
#include "../fileops/file_operations.h"
#include "../panel/panel_container.h"
#include "../panel/panel_widget.h"
#include "../ui/volume_menu.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QEvent>
#include <QInputDialog>
#include <QMouseEvent>
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
            this, [this]() {
                refreshPanelActions();
                updateToolbar();
            });
    connect(panelContainer_, &PanelContainer::orientationChanged,
            this, &MainWindow::refreshPanelActions);

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

    // 监听收藏管理器信号
    connect(FavoriteManager::instance(), &FavoriteManager::favoritesChanged,
            this, [this]() { /* 菜单显示时刷新 */ });

    // 应用初始配置（面板可见性、隐藏文件）
    applyPanelConfig();
    applyFileBrowserConfig();

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
    // 卷子菜单（Phase 4：使用 VolumeMenu）
    volumesMenu_ = menu->addMenu(tr("&Volumes"));
    volumesMenu_->setIcon(QIcon::fromTheme(QStringLiteral("drive-harddisk")));
    volumeMenu_ = new VolumeMenu(volumesMenu_);
    connect(volumesMenu_, &QMenu::aboutToShow, this, [this]() {
        volumeMenu_->refresh();
    });
    connect(volumeMenu_, &VolumeMenu::volumeOpenRequested, this, [this](const QString &mp) {
        panelContainer_->activePanel()->addTab(mp, -1);
    });
    connect(volumeMenu_, &VolumeMenu::volumeMountFailed, this, [this](const QString &err) {
        QMessageBox::warning(this, tr("Mount Failed"), err);
    });
    connect(volumeMenu_, &VolumeMenu::volumeUnmountRequested, this, [this](const QString &dev) {
        QString err;
        if (!VolumeManager::instance()->unmount(dev, &err)) {
            QMessageBox::warning(this, tr("Unmount Failed"), err);
        }
    });
    connect(volumeMenu_, &VolumeMenu::volumeEjectRequested, this, [this](const QString &dev) {
        QString err;
        if (!VolumeManager::instance()->eject(dev, &err)) {
            QMessageBox::warning(this, tr("Eject Failed"), err);
        }
    });
    // 第一次添加占位项，aboutToShow 时刷新
    auto *volPlaceholder = volumesMenu_->addAction(tr("(Loading...)"));
    volPlaceholder->setEnabled(false);

    menu->addSeparator();

    auto *newTabAction = menu->addAction(tr("New &Tab"), QKeySequence(Qt::CTRL | Qt::Key_T),
                                          this, &MainWindow::onNewTab);
    newTabAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    ShortcutManager::instance()->applyToAction(newTabAction, QStringLiteral("file.new_tab"));

    auto *closeTabAction = menu->addAction(tr("&Close Tab"), QKeySequence(Qt::CTRL | Qt::Key_W),
                                            this, &MainWindow::onCloseTab);
    closeTabAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    ShortcutManager::instance()->applyToAction(closeTabAction, QStringLiteral("file.close_tab"));

    auto *cloneTabAction = menu->addAction(tr("&Clone Tab"),
                                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T),
                                            this, &MainWindow::onCloneTab);
    cloneTabAction->setIcon(QIcon::fromTheme(QStringLiteral("tab-new")));
    ShortcutManager::instance()->applyToAction(cloneTabAction, QStringLiteral("file.clone_tab"));

    menu->addSeparator();

    auto *newFileAction = menu->addAction(tr("New &File"), this, &MainWindow::onNewFile);
    newFileAction->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    ShortcutManager::instance()->applyToAction(newFileAction, QStringLiteral("file.new_file"));

    auto *newFolderAction = menu->addAction(tr("New &Folder"), QKeySequence(Qt::Key_F7),
                                             this, &MainWindow::onNewFolder);
    newFolderAction->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
    ShortcutManager::instance()->applyToAction(newFolderAction, QStringLiteral("file.new_folder"));

    menu->addSeparator();

    auto *exitAction = menu->addAction(tr("&Quit"), QKeySequence::Quit,
                                        this, &MainWindow::onExit);
    exitAction->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));
    ShortcutManager::instance()->applyToAction(exitAction, QStringLiteral("file.quit"));
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
    // 安装事件过滤器，支持右键删除收藏项
    menu->installEventFilter(this);
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
    ShortcutManager::instance()->applyToAction(toggleActive, QStringLiteral("settings.switch_active_panel"));

    toggleOrientationAction_ = menu->addAction(QString(), this,
                                                  &MainWindow::onToggleOrientation);
    ShortcutManager::instance()->applyToAction(toggleOrientationAction_, QStringLiteral("settings.toggle_orientation"));

    menu->addSeparator();

    togglePanel1Action_ = menu->addAction(QString(), this,
                                            &MainWindow::onTogglePanel1Visible);
    ShortcutManager::instance()->applyToAction(togglePanel1Action_, QStringLiteral("settings.toggle_panel1"));

    togglePanel2Action_ = menu->addAction(QString(), this,
                                            &MainWindow::onTogglePanel2Visible);
    ShortcutManager::instance()->applyToAction(togglePanel2Action_, QStringLiteral("settings.toggle_panel2"));

    menu->addSeparator();

    toggleHiddenAction_ = menu->addAction(tr("Show &Hidden Files"),
                                            QKeySequence(Qt::CTRL | Qt::Key_H),
                                            this, &MainWindow::onToggleHiddenFiles);
    toggleHiddenAction_->setCheckable(true);
    ShortcutManager::instance()->applyToAction(toggleHiddenAction_, QStringLiteral("settings.toggle_hidden"));

    menu->addSeparator();

    auto *settingsAction = menu->addAction(tr("&Settings..."), this,
                                            &MainWindow::onOpenSettings);
    settingsAction->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
}

void MainWindow::buildHelpMenu(QMenu *menu) {
    auto *aboutAction = menu->addAction(tr("&About"), this, &MainWindow::onAbout);
    aboutAction->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    ShortcutManager::instance()->applyToAction(aboutAction, QStringLiteral("help.about"));
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

    // 使用 FavoriteManager 获取收藏列表
    const QStringList names = FavoriteManager::instance()->favoriteNames();
    if (names.isEmpty()) {
        auto *placeholder = favoritesMenu_->addAction(tr("(No favorites)"));
        placeholder->setEnabled(false);
        return;
    }

    for (const QString &name : names) {
        auto *action = favoritesMenu_->addAction(name);
        action->setData(name);  // 供右键删除时识别
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
        const bool showHidden = ConfigManager::instance()->value(
            QStringLiteral("File_Browser"), QStringLiteral("showHidden"), false).toBool();
        toggleHiddenAction_->setChecked(showHidden);
    }
}

void MainWindow::updateToolbar() {
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

void MainWindow::restoreSession() {
    auto *cfg = ConfigManager::instance();
    // 读取 [Session]
    const QString sessionData = cfg->value(QStringLiteral("Session"),
                                            QStringLiteral("data")).toString();
    if (sessionData.isEmpty()) return;

    LayoutState state;
    if (!SessionState::deserialize(sessionData, state)) return;

    panelContainer_->setHorizontalSizes(state.horizontalSizes);
    panelContainer_->setVerticalSizes(state.verticalSizes);
    panelContainer_->setOrientation(state.orientation);
    panelContainer_->setPanelVisible(PanelId::Panel1, state.panelVisible[0]);
    panelContainer_->setPanelVisible(PanelId::Panel2, state.panelVisible[1]);
    // 应用当前方向的保存比例（若有）
    const QList<int> &curSizes = (state.orientation == Qt::Horizontal)
        ? state.horizontalSizes : state.verticalSizes;
    if (!curSizes.isEmpty())
        panelContainer_->setSplitterSizes(curSizes);

    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        p->setTabStates(state.panels[i].tabs, state.panels[i].activeTabIndex);
    }
}

void MainWindow::onExit() {
    close();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // 保存布局到 [Session]
    LayoutState state;
    state.orientation = panelContainer_->orientation();
    state.panelVisible[0] = panelContainer_->isPanelVisible(PanelId::Panel1);
    state.panelVisible[1] = panelContainer_->isPanelVisible(PanelId::Panel2);
    // 当前方向的实际比例同步到对应成员，再分别持久化左右/上下比例
    const Qt::Orientation curOri = panelContainer_->orientation();
    const QList<int> curSizes = panelContainer_->splitterSizes();
    if (curOri == Qt::Horizontal) panelContainer_->setHorizontalSizes(curSizes);
    else panelContainer_->setVerticalSizes(curSizes);
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

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    // 收藏菜单右键：弹出删除菜单
    if (obj == favoritesMenu_ && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::RightButton) {
            auto *menu = static_cast<QMenu*>(obj);
            QAction *act = menu->actionAt(me->pos());
            if (act) {
                const QString name = act->data().toString();
                if (!name.isEmpty()) {
                    QMenu ctx(menu);
                    auto *removeAct = ctx.addAction(tr("Remove Favorite"));
                    removeAct->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
                    const QAction *chosen = ctx.exec(me->globalPosition().toPoint());
                    if (chosen == removeAct) {
                        if (FavoriteManager::instance()->removeFavorite(name)) {
                            refreshFavoritesMenu();
                        }
                    }
                    return true;  // 事件已处理
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
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

    // 构建当前布局状态
    LayoutState state;
    state.orientation = panelContainer_->orientation();
    state.panelVisible[0] = panelContainer_->isPanelVisible(PanelId::Panel1);
    state.panelVisible[1] = panelContainer_->isPanelVisible(PanelId::Panel2);
    // 当前方向的实际比例同步到对应成员，再分别持久化左右/上下比例
    {
        const Qt::Orientation ori = panelContainer_->orientation();
        const QList<int> sizes = panelContainer_->splitterSizes();
        if (ori == Qt::Horizontal) panelContainer_->setHorizontalSizes(sizes);
        else panelContainer_->setVerticalSizes(sizes);
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

void MainWindow::onFavoriteTriggered(const QString &name) {
    LayoutState state;
    if (!FavoriteManager::instance()->loadFavorite(name, state)) return;

    panelContainer_->setHorizontalSizes(state.horizontalSizes);
    panelContainer_->setVerticalSizes(state.verticalSizes);
    panelContainer_->setOrientation(state.orientation);
    panelContainer_->setPanelVisible(PanelId::Panel1, state.panelVisible[0]);
    panelContainer_->setPanelVisible(PanelId::Panel2, state.panelVisible[1]);
    // 应用当前方向的保存比例（若有）
    const QList<int> &curSizes = (state.orientation == Qt::Horizontal)
        ? state.horizontalSizes : state.verticalSizes;
    if (!curSizes.isEmpty())
        panelContainer_->setSplitterSizes(curSizes);
    for (int i = 0; i < 2; ++i) {
        auto *p = panelContainer_->panel(static_cast<PanelId>(i));
        p->setTabStates(state.panels[i].tabs, state.panels[i].activeTabIndex);
    }
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
    cfg->setValue(QStringLiteral("File_Browser"), QStringLiteral("showHidden"), show);
    // configChanged 信号会触发 applyFileBrowserConfig
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
    // FmApplication 会监听 configChanged 信号应用主题
}

void MainWindow::onOpenSettings() {
    SettingsDialog dlg(this);
    // 添加四个设置页
    dlg.addPage(new UiSettingsPage(this));
    dlg.addPage(new PanelSettingsPage(this));
    dlg.addPage(new FileBrowserSettingsPage(this));
    dlg.addPage(new ShortcutSettingsPage(this));
    dlg.exec();
}

void MainWindow::applyPanelConfig() {
    auto *cfg = ConfigManager::instance();
    const int orient = cfg->value(QStringLiteral("Panels"), QStringLiteral("orientation"),
                                    static_cast<int>(Qt::Horizontal)).toInt();
    const bool p1Visible = cfg->value(QStringLiteral("Panels"), QStringLiteral("panel1Visible"), true).toBool();
    const bool p2Visible = cfg->value(QStringLiteral("Panels"), QStringLiteral("panel2Visible"), true).toBool();

    panelContainer_->setOrientation(static_cast<Qt::Orientation>(orient));
    panelContainer_->setPanelVisible(PanelId::Panel1, p1Visible);
    panelContainer_->setPanelVisible(PanelId::Panel2, p2Visible);
    // 活动面板由会话恢复，不在此处设置
}

void MainWindow::applyFileBrowserConfig() {
    auto *cfg = ConfigManager::instance();
    const bool showHidden = cfg->value(QStringLiteral("File_Browser"),
                                         QStringLiteral("showHidden"), false).toBool();
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

void MainWindow::onAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

} // namespace fm
