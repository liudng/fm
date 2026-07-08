#ifndef FM_PANEL_PANEL_CONTAINER_H
#define FM_PANEL_PANEL_CONTAINER_H

#include "../panel/panel_id.h"

#include <QWidget>

class QSplitter;

namespace fm {

class PanelWidget;

// 双面板容器
// - 默认左右布局（QSplitter Horizontal），可切上下（Vertical）
// - 隐藏面板后剩余占满；再次显示恢复比例
// - 活动面板高亮
class PanelContainer : public QWidget {
    Q_OBJECT
public:
    PanelContainer(QWidget *parent = nullptr);

    PanelWidget *panel(PanelId id) const;
    PanelWidget *activePanel() const;
    void setActivePanel(PanelId id);
    PanelId activePanelId() const { return activePanel_; }

    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const;

    void setPanelVisible(PanelId id, bool visible);
    bool isPanelVisible(PanelId id) const;

    // Splitter 比例（用于持久化）
    QList<int> splitterSizes() const;
    void setSplitterSizes(const QList<int> &sizes);
    // 左右/上下布局的比例分别记忆
    QList<int> horizontalSizes() const;
    QList<int> verticalSizes() const;
    void setHorizontalSizes(const QList<int> &sizes);
    void setVerticalSizes(const QList<int> &sizes);

signals:
    void activePanelChanged(PanelId id);
    void panelVisibilityChanged();
    void orientationChanged();

private:
    QSplitter *splitter_;
    PanelWidget *panels_[2] = {nullptr, nullptr};
    PanelId activePanel_ = PanelId::Panel1;
    // 左右/上下布局的比例分别记忆
    QList<int> horizontalSizes_;
    QList<int> verticalSizes_;
    // 面板隐藏前的比例，用于恢复（仅当前方向有效）
    QList<int> hiddenSizes_;
};

} // namespace fm

#endif // FM_PANEL_PANEL_CONTAINER_H
