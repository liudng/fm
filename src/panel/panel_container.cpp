#include "panel_container.h"

#include "../panel/panel_widget.h"

#include <QSplitter>
#include <QVBoxLayout>

namespace fm {

PanelContainer::PanelContainer(QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter_);

    panels_[0] = new PanelWidget(PanelId::Panel1, this);
    panels_[1] = new PanelWidget(PanelId::Panel2, this);
    splitter_->addWidget(panels_[0]);
    splitter_->addWidget(panels_[1]);
    splitter_->setSizes({640, 640});  // 默认 50:50
    splitter_->setChildrenCollapsible(false);

    // 点击面板激活
    for (int i = 0; i < 2; ++i) {
        connect(panels_[i], &PanelWidget::activeTabChanged, this, [this, i]() {
            setActivePanel(i == 0 ? PanelId::Panel1 : PanelId::Panel2);
        });
    }
    updateActiveHighlight();
}

PanelWidget *PanelContainer::panel(PanelId id) const {
    return panels_[static_cast<int>(id)];
}

PanelWidget *PanelContainer::activePanel() const {
    return panels_[static_cast<int>(activePanel_)];
}

void PanelContainer::setActivePanel(PanelId id) {
    if (activePanel_ == id) return;
    activePanel_ = id;
    updateActiveHighlight();
    emit activePanelChanged(id);
}

void PanelContainer::setOrientation(Qt::Orientation orientation) {
    const Qt::Orientation oldOri = splitter_->orientation();
    if (oldOri == orientation) return;
    // 保存当前比例到旧方向
    const QList<int> currentSizes = splitter_->sizes();
    if (oldOri == Qt::Horizontal) horizontalSizes_ = currentSizes;
    else verticalSizes_ = currentSizes;
    splitter_->setOrientation(orientation);
    // 从新方向恢复（若曾记录过）
    const QList<int> &target = (orientation == Qt::Horizontal) ? horizontalSizes_ : verticalSizes_;
    if (!target.isEmpty()) splitter_->setSizes(target);
    emit orientationChanged();
}

Qt::Orientation PanelContainer::orientation() const {
    return splitter_->orientation();
}

void PanelContainer::setPanelVisible(PanelId id, bool visible) {
    auto *p = panel(id);
    if (!p) return;
    if (visible == p->isVisible()) return;

    if (!visible) {
        // 至少保持一个可见
        const int otherIdx = 1 - static_cast<int>(id);
        if (!panels_[otherIdx]->isVisible()) return;
        hiddenSizes_ = splitter_->sizes();
        p->hide();
        // 让另一个面板占满
        splitter_->setSizes({1, 1});
    } else {
        p->show();
        // 恢复隐藏前的比例（仍在当前方向）
        splitter_->setSizes(hiddenSizes_.isEmpty() ? QList<int>({1, 1}) : hiddenSizes_);
    }
    emit panelVisibilityChanged();
}

bool PanelContainer::isPanelVisible(PanelId id) const {
    auto *p = panel(id);
    return p && p->isVisible();
}

QList<int> PanelContainer::splitterSizes() const {
    return splitter_->sizes();
}

void PanelContainer::setSplitterSizes(const QList<int> &sizes) {
    splitter_->setSizes(sizes);
}

QList<int> PanelContainer::horizontalSizes() const {
    return horizontalSizes_;
}

QList<int> PanelContainer::verticalSizes() const {
    return verticalSizes_;
}

void PanelContainer::setHorizontalSizes(const QList<int> &sizes) {
    horizontalSizes_ = sizes;
}

void PanelContainer::setVerticalSizes(const QList<int> &sizes) {
    verticalSizes_ = sizes;
}

void PanelContainer::updateActiveHighlight() {
    // 高亮活动面板：通过样式边框
    for (int i = 0; i < 2; ++i) {
        auto *p = panels_[i];
        const bool active = (static_cast<PanelId>(i) == activePanel_);
        p->setStyleSheet(active
            ? QStringLiteral("PanelWidget { border: 2px solid palette(highlight); }")
            : QStringLiteral("PanelWidget { border: 2px solid transparent; }"));
    }
}

} // namespace fm
