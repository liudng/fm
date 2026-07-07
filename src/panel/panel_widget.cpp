#include "panel_widget.h"

#include "../core/clipboard_manager.h"
#include "../core/column_manager.h"
#include "../core/config_manager.h"
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
#include <QIcon>
#include <QMenu>
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

    setStyleSheet(QStringLiteral(
        "QWidget#activePanel { border: 1px solid palette(highlight); }"));

    connect(tabBar_, &QTabBar::currentChanged, this, &PanelWidget::onTabChanged);
    connect(tabBar_, &FileTabBar::newTabRequested, this, [this]() {
        // 新选项卡默认打开活动选项卡同目录
        addTab(activeTabPath(), -1);
    });
    connect(tabBar_, &FileTabBar::closeTabRequested, this, &PanelWidget::closeTab);
    connect(tabBar_, &FileTabBar::contextMenuRequested, this, [this](int index, const QPoint &) {
        // 选项卡上下文菜单在后续阶段实现，先无操作
        Q_UNUSED(index);
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

    // 记录历史
    td.history.clear();
    td.history.append(path);
    td.historyIndex = 0;

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
                bool hasSelection = !view->selectionModel()->selectedRows().isEmpty();
                // 内部直接处理菜单
                showContextMenu(globalPos, hasSelection);
                // 同时通知外部（如主窗口工具栏更新）
                emit contextMenuRequested(globalPos, hasSelection);
            });
    connect(model, &FileListModel::pathChanged, this, [this, index](const QString &p) {
        if (index < tabBar_->count()) tabBar_->setTabPath(index, p);
        emit pathChanged(p);
    });

    emit tabCountChanged();
    return index;
}

void PanelWidget::closeTab(int index) {
    if (tabs_.size() <= 1) return;  // 至少保留一个选项卡
    if (index < 0 || index >= tabs_.size()) return;

    // 先从 ColumnManager 注销
    ColumnManager::instance()->unregisterView(tabs_.at(index).view);

    QWidget *w = stack_->widget(index);
    stack_->removeWidget(w);
    delete w;
    tabBar_->removeTab(index);
    tabs_.removeAt(index);
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
    emit activeTabChanged(index);
    emit pathChanged(tabs_.at(index).model->path());
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

void PanelWidget::setTabStates(const QList<TabState> &states, int activeIndex) {
    // 清空已有选项卡
    while (tabs_.size() > 1) closeTab(tabs_.size() - 1);
    if (!tabs_.isEmpty()) closeTab(0);

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

// 对面面板路径
QString PanelWidget::oppositePanelPath() const {
    auto *container = qobject_cast<PanelContainer *>(parentWidget());
    if (!container) return {};
    const PanelId opp = (id_ == PanelId::Panel1) ? PanelId::Panel2 : PanelId::Panel1;
    auto *oppPanel = container->panel(opp);
    return oppPanel ? oppPanel->activeTabPath() : QString();
}

// 显示右键菜单
void PanelWidget::showContextMenu(const QPoint &globalPos, bool hasSelection) {
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto *cb = ClipboardManager::instance();
    const bool canPaste = cb->hasFiles();

    if (hasSelection) {
        const QList<FileItem> items = selectedItems();
        const bool singleSel = items.size() == 1;
        const FileItem first = items.isEmpty() ? FileItem() : items.first();

        auto *openAction = menu->addAction(tr("&Open"));
        openAction->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
        connect(openAction, &QAction::triggered, this, &PanelWidget::onOpen);

        auto *openWithAction = menu->addAction(tr("Open &With..."));
        openWithAction->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
        connect(openWithAction, &QAction::triggered, this, &PanelWidget::onOpenWith);
        openWithAction->setEnabled(singleSel && !first.isDir);

        menu->addSeparator();

        auto *renameAction = menu->addAction(tr("&Rename"));
        renameAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));
        renameAction->setShortcut(QKeySequence(Qt::Key_F2));
        renameAction->setEnabled(singleSel);
        connect(renameAction, &QAction::triggered, this, &PanelWidget::onRename);

        auto *cutAction = menu->addAction(tr("Cu&t"));
        cutAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-cut")));
        cutAction->setShortcut(QKeySequence::Cut);
        connect(cutAction, &QAction::triggered, this, &PanelWidget::onCut);

        auto *copyAction = menu->addAction(tr("&Copy"));
        copyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        copyAction->setShortcut(QKeySequence::Copy);
        connect(copyAction, &QAction::triggered, this, &PanelWidget::onCopy);

        menu->addSeparator();

        auto *cutToOppAction = menu->addAction(tr("Cut to &Opposite"));
        cutToOppAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-cut")));
        connect(cutToOppAction, &QAction::triggered, this, &PanelWidget::onCutToOpposite);
        cutToOppAction->setEnabled(!oppositePanelPath().isEmpty());

        auto *copyToOppAction = menu->addAction(tr("Copy to O&pposite"));
        copyToOppAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        connect(copyToOppAction, &QAction::triggered, this, &PanelWidget::onCopyToOpposite);
        copyToOppAction->setEnabled(!oppositePanelPath().isEmpty());

        menu->addSeparator();

        auto *copyPathAction = menu->addAction(tr("Copy &Path"));
        copyPathAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        copyPathAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
        connect(copyPathAction, &QAction::triggered, this, &PanelWidget::onCopyPath);

        auto *copyNameAction = menu->addAction(tr("Copy File &Name"));
        copyNameAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        copyNameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
        connect(copyNameAction, &QAction::triggered, this, &PanelWidget::onCopyFileName);

        menu->addSeparator();

        if (canPaste) {
            auto *pasteAction = menu->addAction(tr("&Paste"));
            pasteAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
            pasteAction->setShortcut(QKeySequence::Paste);
            connect(pasteAction, &QAction::triggered, this, &PanelWidget::onPaste);
        }

        auto *trashAction = menu->addAction(tr("Move to &Trash"));
        trashAction->setIcon(QIcon::fromTheme(QStringLiteral("user-trash")));
        trashAction->setShortcut(QKeySequence(Qt::Key_Delete));
        connect(trashAction, &QAction::triggered, this, &PanelWidget::onTrash);

        auto *deleteAction = menu->addAction(tr("&Delete Permanently"));
        deleteAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
        deleteAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Delete));
        connect(deleteAction, &QAction::triggered, this, &PanelWidget::onDeletePermanently);

        menu->addSeparator();

        auto *propsAction = menu->addAction(tr("P&roperties"));
        propsAction->setIcon(QIcon::fromTheme(QStringLiteral("document-properties")));
        propsAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));
        propsAction->setEnabled(singleSel);
        connect(propsAction, &QAction::triggered, this, &PanelWidget::onProperties);
    } else {
        // 无选中：导航 + 新建 + 刷新
        auto *backAction = menu->addAction(tr("&Back"));
        backAction->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
        backAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
        connect(backAction, &QAction::triggered, this, &PanelWidget::navigateBack);

        auto *forwardAction = menu->addAction(tr("&Forward"));
        forwardAction->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
        forwardAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
        connect(forwardAction, &QAction::triggered, this, &PanelWidget::navigateForward);

        auto *upAction = menu->addAction(tr("&Up"));
        upAction->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
        upAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
        connect(upAction, &QAction::triggered, this, &PanelWidget::navigateUp);

        menu->addSeparator();

        auto *newFileAction = menu->addAction(tr("New &File"));
        newFileAction->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
        connect(newFileAction, &QAction::triggered, this, &PanelWidget::onNewFile);

        auto *newFolderAction = menu->addAction(tr("New &Folder"));
        newFolderAction->setIcon(QIcon::fromTheme(QStringLiteral("folder-new")));
        newFolderAction->setShortcut(QKeySequence(Qt::Key_F7));
        connect(newFolderAction, &QAction::triggered, this, &PanelWidget::onNewFolder);

        menu->addSeparator();

        auto *refreshAction = menu->addAction(tr("&Refresh"));
        refreshAction->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
        refreshAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
        connect(refreshAction, &QAction::triggered, this, &PanelWidget::refresh);

        if (canPaste) {
            menu->addSeparator();
            auto *pasteAction = menu->addAction(tr("&Paste"));
            pasteAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
            pasteAction->setShortcut(QKeySequence::Paste);
            connect(pasteAction, &QAction::triggered, this, &PanelWidget::onPaste);
        }
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
    OpenWithDialog dlg(item.mimeTypeName, item.name, this);
    if (dlg.exec() != QDialog::Accepted) return;
    // 简单实现：通过 QDesktopServices 或后续阶段补全真正的 OpenWith 处理
    // 此处仅示意
    Q_UNUSED(item);
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
