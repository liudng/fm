#ifndef FM_CORE_SESSION_STATE_H
#define FM_CORE_SESSION_STATE_H

#include <QList>
#include <QString>
#include <Qt>

namespace fm {

// 单个选项卡的状态
struct TabState {
    QString path;
    int sortColumn = 0;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
};

// 单个面板的状态
struct PanelState {
    QList<TabState> tabs;
    int activeTabIndex = 0;
};

// 整体布局状态
struct LayoutState {
    Qt::Orientation orientation = Qt::Horizontal;  // 横向=左右，纵向=上下
    PanelState panels[2];
    QList<int> horizontalSizes;   // 横向布局时的 splitter 比例（像素值）
    QList<int> verticalSizes;     // 纵向布局时的 splitter 比例（像素值）
    bool panelVisible[2] = {true, true};
};

// 会话状态序列化辅助
class SessionState {
public:
    // 序列化/反序列化用于 [Session] section
    static QString serialize(const LayoutState &state);
    static bool deserialize(const QString &data, LayoutState &outState);

    // 默认布局：双面板各一个选项卡指向 Home
    static LayoutState defaultLayout();
};

} // namespace fm

#endif // FM_CORE_SESSION_STATE_H
