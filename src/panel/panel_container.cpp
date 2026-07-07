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
    if (splitter_->orientation() == orientation) return;
    // 保留比例
    savedSizes_ = splitter_->sizes();
    splitter_->setOrientation(orientation);
    splitter_->setSizes(savedSizes_);
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
        savedSizes_ = splitter_->sizes();
        p->hide();
        // 让另一个面板占满
        splitter_->setSizes({1});
    } else {
        p->show();
        // 恢复比例
        splitter_->setSizes(savedSizes_.isEmpty() ? QList<int>({1, 1}) : savedSizes_);
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
