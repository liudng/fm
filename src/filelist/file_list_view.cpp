#include "file_list_view.h"

#include "../filelist/file_list_model.h"
#include "../filelist/file_list_sort_proxy.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QHeaderView>

namespace fm {

FileListView::FileListView(QWidget *parent)
    : QTreeView(parent) {
    // 选择行为
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setUniformRowHeights(true);
    setAlternatingRowColors(true);
    setSortingEnabled(true);

    // 列头可拖动调整顺序、可拖动调整宽度
    header()->setSectionsMovable(true);
    header()->setSectionsClickable(true);
    header()->setStretchLastSection(false);
}

void FileListView::setColumnConfig(const QList<int> &visibleColumns,
                                   const QMap<int, double> &widthRatios) {
    // 隐藏所有列，然后显示指定列
    auto *model = this->model();
    if (!model) return;
    const int totalCols = model->columnCount();
    for (int c = 0; c < totalCols; ++c) {
        setColumnHidden(c, true);
    }
    int visibleCount = visibleColumns.size();
    for (int idx = 0; idx < visibleColumns.size(); ++idx) {
        const int col = visibleColumns.at(idx);
        if (col < 0 || col >= totalCols) continue;
        setColumnHidden(col, false);
        header()->moveSection(header()->visualIndex(col), idx);
        // 按比例设置列宽
        double ratio = widthRatios.value(col, 0.1);
        const int totalWidth = viewport()->width();
        header()->resizeSection(col, qMax(40, int(totalWidth * ratio)));
    }
}

void FileListView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QTreeView::mouseDoubleClickEvent(event);
        return;
    }
    const QModelIndex proxyIndex = indexAt(event->pos());
    if (!proxyIndex.isValid()) {
        QTreeView::mouseDoubleClickEvent(event);
        return;
    }

    // 检查是否 ".." 行
    auto *proxy = qobject_cast<FileListSortProxy*>(model());
    if (proxy) {
        const QModelIndex srcIndex = proxy->mapToSource(proxyIndex);
        auto *fileModel = qobject_cast<FileListModel*>(proxy->sourceModel());
        if (fileModel && fileModel->isParentRow(srcIndex)) {
            emit parentDirRequested();
            return;
        }
    }

    emit openRequested(proxyIndex);
}

void FileListView::contextMenuEvent(QContextMenuEvent *event) {
    emit contextMenuRequested(event->globalPos());
}

void FileListView::keyPressEvent(QKeyEvent *event) {
    const Qt::KeyboardModifiers mods = event->modifiers();
    const int key = event->key();

    // 以下快捷键已由 PanelWidget 持久化 QAction 处理（WidgetWithChildrenShortcut 上下文），
    // Qt 快捷键系统会消费这些事件，不会进入 keyPressEvent。
    // 这里仅处理没有对应 QAction 的辅助键：

    // Backspace：上一级（无对应快捷键配置项，始终由键盘处理）
    if (key == Qt::Key_Backspace && mods == Qt::NoModifier) {
        emit parentDirRequested();
        return;
    }
    // F5：刷新（Ctrl+R 由 QAction 处理，F5 作为补充）
    if (key == Qt::Key_F5 && mods == Qt::NoModifier) {
        emit refreshRequested();
        return;
    }
    // Ctrl+A：全选（无对应快捷键配置项，始终由键盘处理）
    if (key == Qt::Key_A && mods == Qt::ControlModifier) {
        emit selectAllRequested();
        return;
    }

    // 默认处理（↑/↓ 等导航键）
    QTreeView::keyPressEvent(event);
}

} // namespace fm
