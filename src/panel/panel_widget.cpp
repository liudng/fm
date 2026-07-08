#include "panel_widget.h"

#include "../core/clipboard_manager.h"
#include "../core/column_manager.h"
#include "../core/config_manager.h"
#include "../core/open_with_manager.h"
#include "../core/session_state.h"
#include "../core/shortcut_manager.h"
#include "../dialogs/input_name_dialog.h"
#include "../dialogs/open_with_dialog.h"
#include "../dialogs/properties_dialog.h"
#include "../filelist/file_item.h"
#include "../filelist/file_list_model.h"
#include "../filelist/file_list_sort_proxy.h"
#include "../filelist/file_list_view.h"
#include "../fileops/file_operations.h"
#include "../panel/file_tab_bar.h"
#include "../panel/panel_container.h"
#include "../panel/panel_id.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
#include <QMimeDatabase>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fm {

PanelWidget::PanelWidget(PanelId id, QWidget *parent)
    : QWidget(parent), id_(id) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    tabBar_ = new FileTabBar(this);
    layout->addWidget(tabBar_);

    stack_ = new QStackedWidget(this);
    layout->addWidget(stack_, 1);

    connect(tabBar_, &QTabBar::currentChanged, this, &PanelWidget::onTabChanged);
    connect(tabBar_, &FileTabBar::newTabRequested, this, [this]() {
        // 新选项卡默认打开活动选项卡同目录
        addTab(activeTabPath(), -1);
    });
    connect(tabBar_, &FileTabBar::closeTabRequested, this, &PanelWidget::closeTab);
    connect(tabBar_, &FileTabBar::contextMenuRequested, this, [this](int index, const QPoint &globalPos) {
        QMenu menu(this);
        auto *closeAct = menu.addAction(tr("Close"));
        closeAct->setEnabled(tabCount() > 1);  // 最后一个不可关闭
        auto *closeOthersAct = menu.addAction(tr("Close Others"));
        closeOthersAct->setEnabled(tabCount() > 1);
        menu.addAction(tr("Clone"));
        const QAction *chosen = menu.exec(globalPos);
        if (chosen == closeAct) {
            closeTab(index);
        } else if (chosen == closeOthersAct) {
            closeOtherTabs(index);
        } else if (chosen) {
            cloneTab(index);
        }
    });

    // 文件操作完成后自动刷新受影响的目录
    connect(FileOperations::instance(), &FileOperations::directoryChanged,
            this, [this](const QString &dir) {
        // 检查所有选项卡是否在该目录下
        for (const auto &td : std::as_const(tabs_)) {
            if (td.model && td.model->path() == dir) {
                td.model->reload();
            }
        }
    });

    // 监听配置变更（隐藏文件、列设置）
    auto *cfg = ConfigManager::instance();
    connect(cfg, &ConfigManager::configChanged, this, [this](const QString &section) {
        if (section == QStringLiteral("File_Browser")) {
            auto *c = ConfigManager::instance();
            const bool showHidden = c->value(QStringLiteral("File_Browser"),
                                                QStringLiteral("showHidden"), false).toBool();
            for (const auto &td : std::as_const(tabs_)) {
                if (td.model) td.model->setShowHidden(showHidden);
            }
        } else if (section == QStringLiteral("File_Browser_Columns")) {
            for (const auto &td : std::as_const(tabs_)) {
                ColumnManager::instance()->applyToView(td.view);
            }
        }
    });

    // 创建持久化动作（用于工具栏与右键菜单共享）
    createActions();
}

void PanelWidget::applyColumnConfig(FileListView *view) {
    // 通过 ColumnManager 应用配置
    ColumnManager::instance()->applyToView(view);
}

int PanelWidget::addTab(const QString &path, int index) {
    if (path.isEmpty()) return -1;

    auto *view = new FileListView(this);
    auto *model = new FileListModel(view);
    auto *proxy = new FileListSortProxy(view);
    proxy->setSourceModel(model);
    view->setModel(proxy);
    applyColumnConfig(view);

    TabData td;
    td.view = view;
    td.model = model;
    td.proxy = proxy;

    // 记录历史（必须在 append/insert 之前设置：QList 隐式共享（COW），
    // 若在 append 之后再修改 td.history 会触发 detach，
    // 导致 tabs_[index].history 仍为空，后退按钮首次操作后仍禁用）
    td.history.append(path);
    td.historyIndex = 0;

    if (index < 0 || index > tabs_.size()) {
        index = tabs_.size();
        tabs_.append(td);
    } else {
        tabs_.insert(index, td);
    }

    stack_->addWidget(view);
    tabBar_->insertTab(index, QStringLiteral("..."));

    // 设置路径
    model->setPath(path);
    tabBar_->setTabPath(index, path);

    // 默认按 Name 排序
    proxy->sort(FileListModel::ColName, Qt::AscendingOrder);

    // 应用隐藏文件配置
    auto *cfg = ConfigManager::instance();
    const bool showHidden = cfg->value(QStringLiteral("File_Browser"),
                                          QStringLiteral("showHidden"), false).toBool();
    model->setShowHidden(showHidden);

    // 注册到 ColumnManager
    ColumnManager::instance()->registerView(view);

    // 连接信号
    connect(view, &FileListView::openRequested, this, &PanelWidget::onOpenRequested);
    connect(view, &FileListView::parentDirRequested, this, &PanelWidget::onParentDirRequested);
    connect(view, &FileListView::contextMenuRequested, this,
            [this, view](const QPoint &globalPos) {
                // 在 ".." 行右键：视为"无选中"（见需求 5.3.1）
                const QPoint viewportPos = view->mapFromGlobal(globalPos);
                const QModelIndex proxyIndex = view->indexAt(viewportPos);
                bool onParentRow = false;
                auto *proxy = qobject_cast<FileListSortProxy*>(view->model());
                if (proxy && proxyIndex.isValid()) {
                    auto *fileModel = qobject_cast<FileListModel*>(proxy->sourceModel());
                    if (fileModel && fileModel->isParentRow(proxy->mapToSource(proxyIndex))) {
                        onParentRow = true;
                    }
                }
                const bool hasSelection = !onParentRow &&
                    !view->selectionModel()->selectedRows().isEmpty();
                // 内部直接处理菜单
                showContextMenu(globalPos, hasSelection);
                // 同时通知外部（如主窗口工具栏更新）
                emit contextMenuRequested(globalPos, hasSelection);
            });

    // 键盘导航信号
    connect(view, &FileListView::openKeyPressed, this, &PanelWidget::onOpen);
    connect(view, &FileListView::renameRequested, this, &PanelWidget::onRename);
    connect(view, &FileListView::refreshRequested, this, &PanelWidget::refresh);
    connect(view, &FileListView::selectAllRequested, view, &FileListView::selectAllFiles);
    connect(view, &FileListView::trashRequested, this, &PanelWidget::onTrash);
    connect(view, &FileListView::deletePermanentlyRequested, this, &PanelWidget::onDeletePermanently);
    connect(view, &FileListView::copyRequested, this, &PanelWidget::onCopy);
    connect(view, &FileListView::cutRequested, this, &PanelWidget::onCut);
    connect(view, &FileListView::pasteRequested, this, &PanelWidget::onPaste);
    connect(view, &FileListView::copyPathRequested, this, &PanelWidget::onCopyPath);
    connect(view, &FileListView::copyFileNameRequested, this, &PanelWidget::onCopyFileName);
    connect(model, &FileListModel::pathChanged, this, [this, index](const QString &p) {
        if (index < tabBar_->count()) tabBar_->setTabPath(index, p);
        emit pathChanged(p);
    });

    // 选中项变化 → 更新动作状态与工具栏
    if (view->selectionModel()) {
        connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
            updateActionStates();
            emit selectionChanged();
        });
    }

    emit tabCountChanged();
    return index;
}

void PanelWidget::closeTab(int index) {
    if (tabs_.size() <= 1) return;  // 至少保留一个选项卡
    if (index < 0 || index >= tabs_.size()) return;

    // 先从 ColumnManager 注销
    ColumnManager::instance()->unregisterView(tabs_.at(index).view);
    // 先从 tabs_ 移除数据，避免后续 removeTab 触发 currentChanged 时访问悬空 view
    FileListView *view = tabs_.at(index).view;
    tabs_.removeAt(index);
    // 从 stack 移除并删除 widget
    stack_->removeWidget(view);
    delete view;
    // 从 tabBar 移除（可能触发 currentChanged；此时 tabs_ 已与新结构一致）
    tabBar_->removeTab(index);
    // 确保 current index 有效（删除当前 tab 后 Qt 会自动选相邻项，这里兜底）
    if (tabBar_->currentIndex() < 0 && tabs_.size() > 0) {
        tabBar_->setCurrentIndex(qMin(index, tabs_.size() - 1));
    }
    emit tabCountChanged();
}

void PanelWidget::closeOtherTabs(int index) {
    if (index < 0 || index >= tabs_.size()) return;
    // 从末尾向开头删除非 index 项
    for (int i = tabs_.size() - 1; i >= 0; --i) {
        if (i != index) closeTab(i);
    }
    // 删除后 index 可能变为 0
    setActiveTab(qMin(index, tabs_.size() - 1));
}

int PanelWidget::cloneTab(int index) {
    if (index < 0 || index >= tabs_.size()) return -1;
    const QString path = tabPath(index);
    return addTab(path, index + 1);
}

int PanelWidget::tabCount() const { return tabs_.size(); }

int PanelWidget::activeTabIndex() const { return tabBar_->currentIndex(); }

void PanelWidget::setActiveTab(int index) {
    if (index >= 0 && index < tabs_.size()) {
        tabBar_->setCurrentIndex(index);
    }
}

QString PanelWidget::tabPath(int index) const {
    if (index < 0 || index >= tabs_.size()) return {};
    return tabs_.at(index).model->path();
}

QString PanelWidget::activeTabPath() const {
    return tabPath(activeTabIndex());
}

FileListView *PanelWidget::listView() const {
    int idx = activeTabIndex();
    if (idx < 0 || idx >= tabs_.size()) return nullptr;
    return tabs_.at(idx).view;
}

FileListModel *PanelWidget::model() const {
    int idx = activeTabIndex();
    if (idx < 0 || idx >= tabs_.size()) return nullptr;
    return tabs_.at(idx).model;
}

FileListSortProxy *PanelWidget::proxyModel() const {
    int idx = activeTabIndex();
    if (idx < 0 || idx >= tabs_.size()) return nullptr;
    return tabs_.at(idx).proxy;
}

FileTabBar *PanelWidget::tabBar() const { return tabBar_; }

void PanelWidget::onTabChanged(int index) {
    if (index < 0 || index >= tabs_.size()) return;
    stack_->setCurrentIndex(index);
    updateActionStates();
    emit activeTabChanged(index);
    emit pathChanged(tabs_.at(index).model->path());
    emit selectionChanged();
}

void PanelWidget::navigateTo(const QString &path, bool addHistory) {
    int idx = activeTabIndex();
    if (idx < 0 || idx >= tabs_.size()) return;
    auto &td = tabs_[idx];
    td.model->setPath(path);
    if (addHistory) {
        // 截断前进历史
        while (td.history.size() > td.historyIndex + 1) td.history.removeLast();
        td.history.append(path);
        td.historyIndex = td.history.size() - 1;
    }
    // 导航后刷新动作状态（后退/前进按钮的启用/禁用）
    updateActionStates();
}

void PanelWidget::onOpenRequested(const QModelIndex &proxyIndex) {
    auto *proxy = proxyModel();
    if (!proxy) return;
    const QModelIndex src = proxy->mapToSource(proxyIndex);
    auto *m = model();
    if (!m) return;
    const FileItem item = m->itemAt(src);
    if (item.absolutePath.isEmpty()) return;
    if (item.isDir) {
        navigateTo(item.absolutePath, true);
        emit openRequested(item.absolutePath);
    }
    // 文件打开在后续阶段实现
}

void PanelWidget::onParentDirRequested() {
    int idx = activeTabIndex();
    if (idx < 0 || idx >= tabs_.size()) return;
    const QString current = tabs_.at(idx).model->path();
    if (current.isEmpty()) return;
    QDir dir(current);
    if (dir.cdUp()) {
        navigateTo(dir.absolutePath(), true);
        emit parentDirRequested();
    }
}

void PanelWidget::navigateBack() {
    int idx = activeTabIndex();
    if (idx < 0 || idx >= tabs_.size()) return;
    auto &td = tabs_[idx];
    if (td.historyIndex > 0) {
        td.historyIndex--;
        navigateTo(td.history.at(td.historyIndex), false);
    }
}

void PanelWidget::navigateForward() {
    int idx = activeTabIndex();
    if (idx < 0 || idx >= tabs_.size()) return;
    auto &td = tabs_[idx];
    if (td.historyIndex < td.history.size() - 1) {
        td.historyIndex++;
        navigateTo(td.history.at(td.historyIndex), false);
    }
}

void PanelWidget::navigateUp() { onParentDirRequested(); }

void PanelWidget::refresh() {
    int idx = activeTabIndex();
    if (idx < 0 || idx >= tabs_.size()) return;
    tabs_.at(idx).model->reload();
}

void PanelWidget::openPath(const QString &path) {
    navigateTo(path, true);
}

// === 持久化动作 ===

void PanelWidget::createActions() {
    const auto sc = Qt::WidgetWithChildrenShortcut;

    // 导航
    actBack_ = new QAction(QIcon::fromTheme(QStringLiteral("go-previous")), tr("&Back"), this);
    actBack_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    actBack_->setShortcutContext(sc);
    connect(actBack_, &QAction::triggered, this, &PanelWidget::navigateBack);

    actForward_ = new QAction(QIcon::fromTheme(QStringLiteral("go-next")), tr("&Forward"), this);
    actForward_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    actForward_->setShortcutContext(sc);
    connect(actForward_, &QAction::triggered, this, &PanelWidget::navigateForward);

    actUp_ = new QAction(QIcon::fromTheme(QStringLiteral("go-up")), tr("&Up"), this);
    actUp_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    actUp_->setShortcutContext(sc);
    connect(actUp_, &QAction::triggered, this, &PanelWidget::navigateUp);

    // 新建
    actNewFile_ = new QAction(QIcon::fromTheme(QStringLiteral("document-new")), tr("New &File"), this);
    actNewFile_->setShortcutContext(sc);
    connect(actNewFile_, &QAction::triggered, this, &PanelWidget::onNewFile);

    actNewFolder_ = new QAction(QIcon::fromTheme(QStringLiteral("folder-new")), tr("New &Folder"), this);
    actNewFolder_->setShortcut(QKeySequence(Qt::Key_F7));
    actNewFolder_->setShortcutContext(sc);
    connect(actNewFolder_, &QAction::triggered, this, &PanelWidget::onNewFolder);

    actRefresh_ = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("&Refresh"), this);
    actRefresh_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    actRefresh_->setShortcutContext(sc);
    connect(actRefresh_, &QAction::triggered, this, &PanelWidget::refresh);

    // 文件操作
    actOpen_ = new QAction(QIcon::fromTheme(QStringLiteral("document-open")), tr("&Open"), this);
    actOpen_->setShortcutContext(sc);
    connect(actOpen_, &QAction::triggered, this, &PanelWidget::onOpen);

    actOpenWith_ = new QAction(QIcon::fromTheme(QStringLiteral("document-open")), tr("Open &With..."), this);
    actOpenWith_->setShortcutContext(sc);
    connect(actOpenWith_, &QAction::triggered, this, &PanelWidget::onOpenWith);

    actRename_ = new QAction(QIcon::fromTheme(QStringLiteral("document-save-as")), tr("&Rename"), this);
    actRename_->setShortcut(QKeySequence(Qt::Key_F2));
    actRename_->setShortcutContext(sc);
    connect(actRename_, &QAction::triggered, this, &PanelWidget::onRename);

    actCut_ = new QAction(QIcon::fromTheme(QStringLiteral("edit-cut")), tr("Cu&t"), this);
    actCut_->setShortcut(QKeySequence::Cut);
    actCut_->setShortcutContext(sc);
    connect(actCut_, &QAction::triggered, this, &PanelWidget::onCut);

    actCopy_ = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("&Copy"), this);
    actCopy_->setShortcut(QKeySequence::Copy);
    actCopy_->setShortcutContext(sc);
    connect(actCopy_, &QAction::triggered, this, &PanelWidget::onCopy);

    actPaste_ = new QAction(QIcon::fromTheme(QStringLiteral("edit-paste")), tr("&Paste"), this);
    actPaste_->setShortcut(QKeySequence::Paste);
    actPaste_->setShortcutContext(sc);
    connect(actPaste_, &QAction::triggered, this, &PanelWidget::onPaste);

    actCutToOpp_ = new QAction(QIcon::fromTheme(QStringLiteral("edit-cut")), tr("Cut to &Opposite"), this);
    actCutToOpp_->setShortcutContext(sc);
    connect(actCutToOpp_, &QAction::triggered, this, &PanelWidget::onCutToOpposite);

    actCopyToOpp_ = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copy to O&pposite"), this);
    actCopyToOpp_->setShortcutContext(sc);
    connect(actCopyToOpp_, &QAction::triggered, this, &PanelWidget::onCopyToOpposite);

    actCopyPath_ = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copy &Path"), this);
    actCopyPath_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    actCopyPath_->setShortcutContext(sc);
    connect(actCopyPath_, &QAction::triggered, this, &PanelWidget::onCopyPath);

    actCopyName_ = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copy File &Name"), this);
    actCopyName_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    actCopyName_->setShortcutContext(sc);
    connect(actCopyName_, &QAction::triggered, this, &PanelWidget::onCopyFileName);

    actTrash_ = new QAction(QIcon::fromTheme(QStringLiteral("user-trash")), tr("Move to &Trash"), this);
    actTrash_->setShortcut(QKeySequence(Qt::Key_Delete));
    actTrash_->setShortcutContext(sc);
    connect(actTrash_, &QAction::triggered, this, &PanelWidget::onTrash);

    actDelete_ = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), tr("&Delete Permanently"), this);
    actDelete_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
    actDelete_->setShortcutContext(sc);
    connect(actDelete_, &QAction::triggered, this, &PanelWidget::onDeletePermanently);

    actProperties_ = new QAction(QIcon::fromTheme(QStringLiteral("document-properties")), tr("P&roperties"), this);
    actProperties_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
    actProperties_->setShortcutContext(sc);
    connect(actProperties_, &QAction::triggered, this, &PanelWidget::onProperties);

    // 通过 ShortcutManager 应用用户自定义快捷键（覆盖默认值）
    auto *sm = ShortcutManager::instance();
    sm->applyToAction(actBack_,         QStringLiteral("filelist.back"));
    sm->applyToAction(actForward_,      QStringLiteral("filelist.forward"));
    sm->applyToAction(actUp_,           QStringLiteral("filelist.up"));
    sm->applyToAction(actNewFile_,      QStringLiteral("file.new_file"));
    sm->applyToAction(actNewFolder_,    QStringLiteral("file.new_folder"));
    sm->applyToAction(actRefresh_,      QStringLiteral("filelist.refresh"));
    sm->applyToAction(actOpen_,         QStringLiteral("filelist.open"));
    sm->applyToAction(actOpenWith_,     QStringLiteral("filelist.open_with"));
    sm->applyToAction(actRename_,       QStringLiteral("filelist.rename"));
    sm->applyToAction(actCut_,          QStringLiteral("filelist.cut"));
    sm->applyToAction(actCopy_,         QStringLiteral("filelist.copy"));
    sm->applyToAction(actPaste_,        QStringLiteral("filelist.paste"));
    sm->applyToAction(actCutToOpp_,     QStringLiteral("filelist.cut_to_opposite"));
    sm->applyToAction(actCopyToOpp_,    QStringLiteral("filelist.copy_to_opposite"));
    sm->applyToAction(actCopyPath_,     QStringLiteral("filelist.copy_path"));
    sm->applyToAction(actCopyName_,     QStringLiteral("filelist.copy_name"));
    sm->applyToAction(actTrash_,       QStringLiteral("filelist.trash"));
    sm->applyToAction(actDelete_,      QStringLiteral("filelist.delete"));
    sm->applyToAction(actProperties_,   QStringLiteral("filelist.properties"));

    updateActionStates();
}

QList<QAction*> PanelWidget::toolbarActions() const {
    // 顺序：后退、前进、向上、刷新 | 新建文件、新建文件夹 | 打开、打开...
    // | 重命名、剪切、复制、粘贴、剪切到对面、复制到对面、复制路径、复制文件名
    // | 移到回收站、彻底删除、属性
    return {
        actBack_, actForward_, actUp_, actRefresh_, nullptr,
        actNewFile_, actNewFolder_, nullptr,
        actOpen_, actOpenWith_, nullptr,
        actRename_, actCut_, actCopy_, actPaste_, actCutToOpp_, actCopyToOpp_,
        actCopyPath_, actCopyName_, nullptr,
        actTrash_, actDelete_, actProperties_
    };
}

void PanelWidget::updateActionStates() {
    const QList<FileItem> items = selectedItems();
    const bool hasSel = !items.isEmpty();
    const bool singleSel = items.size() == 1;
    const FileItem first = items.isEmpty() ? FileItem() : items.first();
    const bool canPaste = ClipboardManager::instance()->hasFiles();
    const bool oppVisible = oppositePanelVisible();

    int idx = activeTabIndex();
    bool canBack = false, canForward = false;
    if (idx >= 0 && idx < tabs_.size()) {
        canBack = tabs_[idx].historyIndex > 0;
        canForward = tabs_[idx].historyIndex < tabs_[idx].history.size() - 1;
    }

    actBack_->setEnabled(canBack);
    actForward_->setEnabled(canForward);
    actUp_->setEnabled(true);
    actNewFile_->setEnabled(true);
    actNewFolder_->setEnabled(true);
    actRefresh_->setEnabled(true);
    actOpen_->setEnabled(singleSel);
    actOpenWith_->setEnabled(singleSel && !first.isDir);
    actRename_->setEnabled(singleSel);
    actCut_->setEnabled(hasSel);
    actCopy_->setEnabled(hasSel);
    actPaste_->setEnabled(canPaste);
    actCutToOpp_->setEnabled(hasSel && oppVisible);
    actCopyToOpp_->setEnabled(hasSel && oppVisible);
    actCopyPath_->setEnabled(hasSel);
    actCopyName_->setEnabled(hasSel);
    actTrash_->setEnabled(hasSel);
    actDelete_->setEnabled(hasSel);
    actProperties_->setEnabled(singleSel);
}

bool PanelWidget::hasSelection() const {
    auto *view = listView();
    if (!view || !view->selectionModel()) return false;
    return !view->selectionModel()->selectedRows().isEmpty();
}

bool PanelWidget::hasSingleSelection() const {
    return selectedItems().size() == 1;
}

bool PanelWidget::oppositePanelVisible() const {
    auto *container = findContainer();
    if (!container) return false;
    const PanelId opp = (id_ == PanelId::Panel1) ? PanelId::Panel2 : PanelId::Panel1;
    return container->isPanelVisible(opp);
}

void PanelWidget::setActivePanel(bool active) {
    if (isActivePanel_ == active) return;
    isActivePanel_ = active;
    if (tabBar_) {
        tabBar_->setStyleSheet(active
            ? QStringLiteral("QTabBar::tab:selected { font-weight: bold; }")
            : QString());
    }
}

QList<TabState> PanelWidget::tabStates() const {
    QList<TabState> states;
    for (const auto &td : tabs_) {
        TabState s;
        s.path = td.model->path();
        // 排序列/顺序在后续阶段记录
        states.append(s);
    }
    return states;
}

void PanelWidget::clearAllTabs() {
    // 阻塞 tabBar 信号，避免删除过程中 currentChanged 访问已删除的 view
    const QSignalBlocker blocker(tabBar_);
    while (!tabs_.isEmpty()) {
        FileListView *view = tabs_.last().view;
        ColumnManager::instance()->unregisterView(view);
        tabs_.removeLast();
        stack_->removeWidget(view);
        delete view;
        tabBar_->removeTab(tabBar_->count() - 1);
    }
    emit tabCountChanged();
}

void PanelWidget::setTabStates(const QList<TabState> &states, int activeIndex) {
    // 先清空所有选项卡（包括最后一个）
    clearAllTabs();

    // 重新添加
    for (const auto &s : states) {
        addTab(s.path, -1);
    }
    if (!states.isEmpty()) {
        setActiveTab(qBound(0, activeIndex, states.size() - 1));
    }
}

// 选中文件项
QList<FileItem> PanelWidget::selectedItems() const {
    QList<FileItem> items;
    auto *view = listView();
    auto *proxy = proxyModel();
    auto *m = model();
    if (!view || !proxy || !m) return items;
    const QModelIndexList rows = view->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIdx : rows) {
        const QModelIndex srcIdx = proxy->mapToSource(proxyIdx);
        if (!m->isParentRow(srcIdx)) {
            items.append(m->itemAt(srcIdx));
        }
    }
    return items;
}

QList<QUrl> PanelWidget::selectedUrls() const {
    QList<QUrl> urls;
    for (const FileItem &item : selectedItems()) {
        if (!item.absolutePath.isEmpty())
            urls.append(QUrl::fromLocalFile(item.absolutePath));
    }
    return urls;
}

// 当前目录
QString PanelWidget::currentDir() const {
    auto *m = model();
    return m ? m->path() : QString();
}

// 向上遍历父链查找 PanelContainer
// PanelWidget 被 QSplitter::addWidget 重定向到 splitter，
// 所以 parentWidget() 返回 QSplitter 而非 PanelContainer
PanelContainer *PanelWidget::findContainer() const {
    auto *p = parentWidget();
    while (p) {
        if (auto *c = qobject_cast<PanelContainer *>(p)) return c;
        p = p->parentWidget();
    }
    return nullptr;
}

// 对面面板路径
QString PanelWidget::oppositePanelPath() const {
    auto *container = findContainer();
    if (!container) return {};
    const PanelId opp = (id_ == PanelId::Panel1) ? PanelId::Panel2 : PanelId::Panel1;
    auto *oppPanel = container->panel(opp);
    return oppPanel ? oppPanel->activeTabPath() : QString();
}

// 显示右键菜单
void PanelWidget::showContextMenu(const QPoint &globalPos, bool hasSelection) {
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // 弹出前刷新动作状态（粘贴项的 enabled 由 canPaste 决定）
    updateActionStates();

    if (hasSelection) {
        // 顺序与工具栏一致：打开、打开...、重命名、剪切、复制、粘贴、
        // 剪切到对面、复制到对面、复制路径、复制文件名、移到回收站、彻底删除、属性
        menu->addAction(actOpen_);
        menu->addAction(actOpenWith_);
        menu->addSeparator();
        menu->addAction(actRename_);
        menu->addAction(actCut_);
        menu->addAction(actCopy_);
        menu->addAction(actPaste_);
        menu->addAction(actCutToOpp_);
        menu->addAction(actCopyToOpp_);
        menu->addSeparator();
        menu->addAction(actCopyPath_);
        menu->addAction(actCopyName_);
        menu->addSeparator();
        menu->addAction(actTrash_);
        menu->addAction(actDelete_);
        menu->addSeparator();
        menu->addAction(actProperties_);
    } else {
        // 无选中：导航 + 新建 + 粘贴（粘贴一律显示，enabled 由状态控制）
        menu->addAction(actBack_);
        menu->addAction(actForward_);
        menu->addAction(actUp_);
        menu->addAction(actRefresh_);
        menu->addSeparator();
        menu->addAction(actNewFile_);
        menu->addAction(actNewFolder_);
        menu->addSeparator();
        menu->addAction(actPaste_);
    }

    menu->popup(globalPos);
}

// === 右键菜单动作槽 ===

void PanelWidget::onOpen() {
    auto *m = model();
    auto *view = listView();
    if (!m || !view) return;
    const QList<FileItem> items = selectedItems();
    if (items.isEmpty()) return;
    const FileItem item = items.first();
    if (item.isDir) {
        navigateTo(item.absolutePath, true);
        emit openRequested(item.absolutePath);
    } else {
        FileOperations::instance()->openWithDefault(QUrl::fromLocalFile(item.absolutePath));
    }
}

void PanelWidget::onOpenWith() {
    const QList<FileItem> items = selectedItems();
    if (items.size() != 1 || items.first().isDir) return;
    const FileItem item = items.first();

    // 获取 MIME 类型
    QMimeDatabase db;
    const QString mimeType = db.mimeTypeForFile(item.absolutePath).name();

    OpenWithDialog dlg(mimeType, item.name, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString app = dlg.selectedApplication();
    if (app.isEmpty()) return;

    // 若勾选"记住此选择"，保存到 [OpenWith]
    if (dlg.rememberChoice()) {
        OpenWithManager::instance()->setDefaultApplication(mimeType, app);
    }

    // 区分 .desktop 应用与自定义命令
    if (app.endsWith(QStringLiteral(".desktop"))) {
        FileOperations::instance()->openWithApplication(QUrl::fromLocalFile(item.absolutePath), app);
    } else {
        FileOperations::instance()->openWithCommand(QUrl::fromLocalFile(item.absolutePath), app);
    }
}

void PanelWidget::onCopy() {
    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) return;
    ClipboardManager::instance()->setFiles(urls, ClipboardManager::Mode::Copy);
}

void PanelWidget::onCut() {
    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) return;
    ClipboardManager::instance()->setFiles(urls, ClipboardManager::Mode::Cut);
}

void PanelWidget::onPaste() {
    const QString dir = currentDir();
    if (dir.isEmpty()) return;
    FileOperations::instance()->pasteFromClipboard(dir);
}

void PanelWidget::onCopyToOpposite() {
    const QString dest = oppositePanelPath();
    if (dest.isEmpty()) return;
    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) return;
    FileOperations::instance()->copy(urls, dest);
}

void PanelWidget::onCutToOpposite() {
    const QString dest = oppositePanelPath();
    if (dest.isEmpty()) return;
    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) return;
    FileOperations::instance()->move(urls, dest);
}

void PanelWidget::onTrash() {
    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) return;
    FileOperations::instance()->trash(urls);
}

void PanelWidget::onDeletePermanently() {
    const QList<QUrl> urls = selectedUrls();
    if (urls.isEmpty()) return;
    FileOperations::instance()->deletePermanently(urls);
}

void PanelWidget::onRename() {
    const QList<FileItem> items = selectedItems();
    if (items.size() != 1) return;
    const FileItem item = items.first();

    // 收集同目录下其他文件名（用于重名校验）
    QStringList existing;
    auto *m = model();
    if (m) {
        const int rows = m->rowCount();
        for (int i = 0; i < rows; ++i) {
            const QModelIndex idx = m->index(i, FileListModel::ColName);
            const QString n = idx.data().toString();
            if (n != item.name && n != QStringLiteral("..")) existing.append(n);
        }
    }

    InputNameDialog dlg(tr("Rename"), tr("New name:"), item.name, this);
    dlg.setExistingNames(existing);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString newName = dlg.name();
    if (newName.isEmpty() || newName == item.name) return;

    FileOperations::instance()->rename(QUrl::fromLocalFile(item.absolutePath), newName);
}

void PanelWidget::onProperties() {
    const QList<FileItem> items = selectedItems();
    if (items.size() != 1) return;
    PropertiesDialog dlg(items.first(), this);
    dlg.exec();
}

void PanelWidget::onCopyPath() {
    const QList<FileItem> items = selectedItems();
    if (items.isEmpty()) return;
    QStringList paths;
    paths.reserve(items.size());
    for (const FileItem &it : items) paths << it.absolutePath;
    QApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
}

void PanelWidget::onCopyFileName() {
    const QList<FileItem> items = selectedItems();
    if (items.isEmpty()) return;
    QStringList names;
    names.reserve(items.size());
    for (const FileItem &it : items) names << it.name;
    QApplication::clipboard()->setText(names.join(QLatin1Char('\n')));
}

void PanelWidget::onNewFile() {
    const QString dir = currentDir();
    if (dir.isEmpty()) return;
    const QString defaultName = tr("New File");
    InputNameDialog dlg(tr("New File"), tr("File name:"), defaultName, this);

    // 重名校验
    QStringList existing = QDir(dir).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    dlg.setExistingNames(existing);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = dlg.name();
    if (name.isEmpty()) return;
    FileOperations::instance()->createFile(dir, name);
}

void PanelWidget::onNewFolder() {
    const QString dir = currentDir();
    if (dir.isEmpty()) return;
    const QString defaultName = tr("New Folder");
    InputNameDialog dlg(tr("New Folder"), tr("Folder name:"), defaultName, this);

    QStringList existing = QDir(dir).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    dlg.setExistingNames(existing);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = dlg.name();
    if (name.isEmpty()) return;
    FileOperations::instance()->createDir(dir, name);
}

} // namespace fm
