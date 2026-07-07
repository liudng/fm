#include "panel_widget.h"

#include "../core/config_manager.h"
#include "../core/session_state.h"
#include "../filelist/file_list_model.h"
#include "../filelist/file_list_sort_proxy.h"
#include "../filelist/file_list_view.h"
#include "../panel/file_tab_bar.h"
#include "../panel/panel_id.h"

#include <QDir>
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
}

void PanelWidget::applyColumnConfig(FileListView *view) {
    // Phase 1：默认四列 Icon/Name/Size/Modified
    QList<int> cols = {FileListModel::ColIcon, FileListModel::ColName,
                       FileListModel::ColSize, FileListModel::ColModified};
    QMap<int, double> ratios;
    ratios[FileListModel::ColIcon] = 0.08;
    ratios[FileListModel::ColName] = 0.42;
    ratios[FileListModel::ColSize] = 0.20;
    ratios[FileListModel::ColModified] = 0.30;
    view->setColumnConfig(cols, ratios);
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

    // 连接信号
    connect(view, &FileListView::openRequested, this, &PanelWidget::onOpenRequested);
    connect(view, &FileListView::parentDirRequested, this, &PanelWidget::onParentDirRequested);
    connect(view, &FileListView::contextMenuRequested, this,
            [this, view](const QPoint &pos) {
                bool hasSelection = !view->selectionModel()->selectedRows().isEmpty();
                emit contextMenuRequested(pos, hasSelection);
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

} // namespace fm
