#include "main_window.h"

#include "../core/config_manager.h"
#include "../core/session_state.h"
#include "../panel/panel_container.h"
#include "../panel/panel_widget.h"

#include <QAction>
#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>
#include <QToolBar>

namespace fm {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("fm"));
    resize(1280, 800);

    // 工具栏（Phase 1 仅占位，后续阶段填充）
    auto *toolbar = addToolBar(QStringLiteral("Toolbar"));
    toolbar->setMovable(false);

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
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenuBar() {
    // 文件菜单
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    // 卷列表在后续阶段补全
    fileMenu->addSeparator();
    auto *exitAction = fileMenu->addAction(tr("&Quit"), this, &MainWindow::onExit,
                                            QKeySequence::Quit);
    exitAction->setIcon(QIcon::fromTheme(QStringLiteral("application-exit")));

    // 收藏菜单（Phase 1 仅占位）
    menuBar()->addMenu(tr("F&avorites"));

    // 设置菜单（Phase 1 仅占位）
    auto *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    settingsMenu->addAction(tr("&Quit"), this, &MainWindow::onExit);  // 占位
    // 后续阶段补全

    // 帮助菜单
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this]() {
        QMessageBox::about(this, tr("About fm"),
            tr("<h3>fm</h3>"
               "<p>Linux dual-panel file manager</p>"
               "<p>Version: 1.0.0</p>"
               "<p>Author: fm team</p>"
               "<p>Copyright (C) 2026 fm team</p>"));
    });
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

} // namespace fm
