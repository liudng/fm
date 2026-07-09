#include "main_window.h"

#include "../core/column_manager.h"
#include "../core/config_manager.h"
#include "../core/favorite_manager.h"
#include "../core/session_state.h"
#include "../core/shortcut_manager.h"
#include "../core/volume_manager.h"
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
#include <QEvent>
#include <QInputDialog>
#include <QMouseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>
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
    fileMenu_ = menu;
    // 卷项在 aboutToShow 时动态插入到 volSeparator_ 之前；
    // 外部设备项动态插入到 volSeparator_ 与 extSeparator_ 之间
    volSeparator_ = menu->addSeparator();
    extSeparator_ = menu->addSeparator();
    connect(menu, &QMenu::aboutToShow, this, &MainWindow::refreshFileMenuVolumes);
    // 安装事件过滤器，支持右键挂载/卸载/弹出
    menu->installEventFilter(this);

    auto *exitAction = menu->addAction(tr("&Quit"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExit);
    exitAction->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));
    ShortcutManager::instance()->applyToAction(exitAction, QStringLiteral("file.quit"));
}

void MainWindow::refreshFileMenuVolumes() {
    if (!fileMenu_ || !volSeparator_ || !extSeparator_) return;

    // 移除旧卷项
    for (QAction *a : volActions_) {
        fileMenu_->removeAction(a);
        a->deleteLater();
    }
    volActions_.clear();

    // 移除旧外部设备项
    for (QAction *a : extActions_) {
        fileMenu_->removeAction(a);
        a->deleteLater();
    }
    extActions_.clear();

    // === 卷段（已挂载卷，QStorageInfo）===
    const QList<VolumeInfo> volumes = VolumeManager::instance()->listVolumes();
    if (volumes.isEmpty()) {
        auto *placeholder = fileMenu_->addAction(tr("(No volumes)"));
        placeholder->setEnabled(false);
        fileMenu_->insertAction(volSeparator_, placeholder);
        volActions_.append(placeholder);
    } else {
        for (const VolumeInfo &v : volumes) {
            QString text = v.label.isEmpty() ? v.mountPoint : v.label;
            if (!v.mountPoint.isEmpty() && text != v.mountPoint) {
                text += QStringLiteral("  (%1)").arg(v.mountPoint);
            }
            auto *act = new QAction(QIcon::fromTheme(QStringLiteral("drive-harddisk")), text, fileMenu_);
            // data 存储挂载点（左键导航用）；deviceFile/isMounted 供右键用
            act->setData(v.mountPoint);
            act->setProperty("deviceFile", v.deviceFile);
            act->setProperty("isMounted", true);  // 卷项均为已挂载
            connect(act, &QAction::triggered, this, [this, mp = v.mountPoint]() {
                // 在活动面板的活动选项卡中打开挂载点（不新建选项卡）
                auto *p = panelContainer_->activePanel();
                if (p) p->openPath(mp);
            });
            fileMenu_->insertAction(volSeparator_, act);
            volActions_.append(act);
        }
    }

    // === 外部设备段（含未挂载，UDisks2）===
    const QList<VolumeInfo> devices = VolumeManager::instance()->listExternalDevices();
    if (devices.isEmpty()) {
        auto *placeholder = fileMenu_->addAction(tr("(No external devices)"));
        placeholder->setEnabled(false);
        fileMenu_->insertAction(extSeparator_, placeholder);
        extActions_.append(placeholder);
    } else {
        for (const VolumeInfo &d : devices) {
            // 设备名称：设备文件名必显示；有卷标/型号时前置
            QString text = d.deviceFile;
            if (!d.label.isEmpty()) {
                text = QStringLiteral("%1 (%2)").arg(d.label, d.deviceFile);
            }
            // 已挂载追加挂载点
            if (d.isMounted && !d.mountPoint.isEmpty()) {
                text += QStringLiteral("  (%1)").arg(d.mountPoint);
            }
            auto *act = new QAction(QIcon::fromTheme(QStringLiteral("drive-removable-media")), text, fileMenu_);
            act->setProperty("deviceFile", d.deviceFile);
            act->setProperty("isMounted", d.isMounted);
            act->setProperty("mountPoint", d.mountPoint);
            // 左键仅选中不操作（不连接 triggered）
            fileMenu_->insertAction(extSeparator_, act);
            extActions_.append(act);
        }
    }
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

    // 根据配置选中当前语言（而非硬编码英文）
    const QString curLang = ConfigManager::instance()->value(
        QStringLiteral("UI"), QStringLiteral("language"), QStringLiteral("en")).toString();
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
    ShortcutManager::instance()->applyToAction(toggleActive, QStringLiteral("settings.switch_active_panel"));

    toggleOrientationAction_ = menu->addAction(QString(), this,
                                                  &MainWindow::onToggleOrientation);
    ShortcutManager::instance()->applyToAction(toggleOrientationAction_, QStringLiteral("settings.toggle_orientation"));

    auto *resetSplitterAction = menu->addAction(tr("&Reset Splitter"), this,
                                                    &MainWindow::onResetSplitter);

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
    settingsAction->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));
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
    // 注意：在窗口 show() 之前调用 setSplitterSizes 通常不会生效
    // （splitter 尺寸为 0，比例计算无意义）。因此延后到事件循环中执行，
    // 此时窗口已 show，并避免被后续 applyPanelConfig 的 setPanelVisible 覆盖。
    const QList<int> curSizes = (state.orientation == Qt::Horizontal)
        ? state.horizontalSizes : state.verticalSizes;
    if (!curSizes.isEmpty()) {
        QTimer::singleShot(0, this, [this, curSizes]() {
            panelContainer_->setSplitterSizes(curSizes);
        });
    }

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
    // 文件菜单卷项/外部设备项右键：挂载/卸载/弹出
    if (obj == fileMenu_ && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::RightButton) {
            auto *menu = static_cast<QMenu*>(obj);
            QAction *act = menu->actionAt(me->pos());
            if (act && (volActions_.contains(act) || extActions_.contains(act))) {
                const QString deviceFile = act->property("deviceFile").toString();
                if (!deviceFile.isEmpty()) {
                    const bool isExternal = extActions_.contains(act);
                    const bool isMounted = act->property("isMounted").toBool();
                    QMenu ctx(menu);
                    QAction *mountAct = nullptr, *unmountAct = nullptr, *ejectAct = nullptr;
                    if (isExternal && !isMounted) {
                        // 未挂载外部设备：仅显示挂载
                        mountAct = ctx.addAction(tr("Mount"));
                        mountAct->setIcon(QIcon::fromTheme(QStringLiteral("media-mount")));
                    } else {
                        // 卷项（已挂载）或已挂载设备项：卸载/弹出
                        unmountAct = ctx.addAction(tr("Safely Unmount"));
                        unmountAct->setIcon(QIcon::fromTheme(QStringLiteral("media-eject")));
                        ejectAct = ctx.addAction(tr("Eject"));
                        ejectAct->setIcon(QIcon::fromTheme(QStringLiteral("media-eject")));
                    }
                    const QAction *chosen = ctx.exec(me->globalPosition().toPoint());
                    QString errMsg;
                    bool ok = false;
                    if (chosen == mountAct) {
                        QString mp = VolumeManager::instance()->mount(deviceFile, &errMsg);
                        ok = !mp.isEmpty();
                    } else if (chosen == unmountAct) {
                        ok = VolumeManager::instance()->unmount(deviceFile, &errMsg);
                    } else if (chosen == ejectAct) {
                        ok = VolumeManager::instance()->eject(deviceFile, &errMsg);
                    } else {
                        return true;  // 取消，仍消费事件
                    }
                    if (!ok && !errMsg.isEmpty()) {
                        QMessageBox::warning(this, tr("Volume Operation Failed"), errMsg);
                    }
                    // 操作成功后刷新整个文件菜单（挂载/卸载后卷段与设备段都会更新）
                    if (ok) refreshFileMenuVolumes();
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

void MainWindow::onResetSplitter() {
    // 将两个面板的比例重置为各占窗口一半
    panelContainer_->setSplitterSizes({1, 1});
    // 同步更新当前方向的记忆比例，避免下次切换方向时恢复为旧值
    const Qt::Orientation curOri = panelContainer_->orientation();
    const QList<int> sizes = panelContainer_->splitterSizes();
    if (curOri == Qt::Horizontal) panelContainer_->setHorizontalSizes(sizes);
    else panelContainer_->setVerticalSizes(sizes);
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
