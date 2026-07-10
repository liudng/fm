#include "session_state.h"

#include <QIODevice>
#include <QMap>
#include <QStandardPaths>
#include <QTextStream>

namespace fm {

QString SessionState::serialize(const LayoutState &state)
{
    // 简易行格式：key=value
    QString out;
    QTextStream ts(&out);
    ts << "orientation=" << (state.orientation == Qt::Horizontal ? "H" : "V") << '\n';
    ts << "panel1Visible=" << (state.panelVisible[0] ? 1 : 0) << '\n';
    ts << "panel2Visible=" << (state.panelVisible[1] ? 1 : 0) << '\n';
    // 左右/上下比例分别记忆
    ts << "horizontal=";
    for (int i = 0; i < state.horizontalSizes.size(); ++i) {
        ts << state.horizontalSizes[i];
        if (i < state.horizontalSizes.size() - 1) ts << ',';
    }
    ts << '\n';
    ts << "vertical=";
    for (int i = 0; i < state.verticalSizes.size(); ++i) {
        ts << state.verticalSizes[i];
        if (i < state.verticalSizes.size() - 1) ts << ',';
    }
    ts << '\n';

    for (int p = 0; p < 2; ++p) {
        const PanelState &panel = state.panels[p];
        ts << "panel" << p << "_activeTab=" << panel.activeTabIndex << '\n';
        ts << "panel" << p << "_tabCount=" << panel.tabs.size() << '\n';
        for (int t = 0; t < panel.tabs.size(); ++t) {
            const TabState &tab = panel.tabs[t];
            ts << "panel" << p << "_tab" << t << "_path=" << tab.path << '\n';
            ts << "panel" << p << "_tab" << t << "_sortCol=" << tab.sortColumn << '\n';
            ts << "panel" << p << "_tab" << t << "_sortOrder=" << tab.sortOrder << '\n';
        }
    }
    return out;
}

static QMap<QString, QString> parseKv(const QString &data)
{
    QMap<QString, QString> map;
    QTextStream ts(const_cast<QString *>(&data), QIODevice::ReadOnly);
    QString line;
    while (ts.readLineInto(&line)) {
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        map.insert(line.left(eq), line.mid(eq + 1));
    }
    return map;
}

static QList<int> parseIntList(const QString &s)
{
    QList<int> result;
    for (const QString &part : s.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        const int v = part.toInt(&ok);
        if (ok) result.append(v);
    }
    return result;
}

bool SessionState::deserialize(const QString &data, LayoutState &outState)
{
    const auto kv = parseKv(data);
    if (kv.isEmpty()) return false;

    outState.orientation = (kv.value("orientation") == "V") ? Qt::Vertical : Qt::Horizontal;
    outState.panelVisible[0] = kv.value("panel1Visible", "1").toInt() != 0;
    outState.panelVisible[1] = kv.value("panel2Visible", "1").toInt() != 0;
    outState.horizontalSizes = parseIntList(kv.value("horizontal"));
    outState.verticalSizes = parseIntList(kv.value("vertical"));

    for (int p = 0; p < 2; ++p) {
        PanelState &panel = outState.panels[p];
        panel.activeTabIndex = kv.value(QStringLiteral("panel%1_activeTab").arg(p), "0").toInt();
        const int tabCount = kv.value(QStringLiteral("panel%1_tabCount").arg(p), "0").toInt();
        panel.tabs.clear();
        for (int t = 0; t < tabCount; ++t) {
            TabState tab;
            tab.path = kv.value(QStringLiteral("panel%1_tab%2_path").arg(p).arg(t));
            tab.sortColumn =
                kv.value(QStringLiteral("panel%1_tab%2_sortCol").arg(p).arg(t), "1").toInt();
            tab.sortOrder = static_cast<Qt::SortOrder>(
                kv.value(QStringLiteral("panel%1_tab%2_sortOrder").arg(p).arg(t), "0").toInt());
            if (!tab.path.isEmpty()) panel.tabs.append(tab);
        }
    }
    return true;
}

LayoutState SessionState::defaultLayout()
{
    LayoutState state;
    state.orientation = Qt::Horizontal;
    state.panelVisible[0] = true;
    state.panelVisible[1] = true;

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    TabState tab;
    tab.path = home;
    tab.sortColumn = 1; // Name
    tab.sortOrder = Qt::AscendingOrder;
    state.panels[0].tabs.append(tab);
    state.panels[1].tabs.append(tab);
    state.panels[0].activeTabIndex = 0;
    state.panels[1].activeTabIndex = 0;
    return state;
}

} // namespace fm
