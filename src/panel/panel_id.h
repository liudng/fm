#ifndef FM_PANEL_PANEL_ID_H
#define FM_PANEL_PANEL_ID_H

#include <QMetaType>

namespace fm {

enum class PanelId {
    Panel1 = 0,
    Panel2 = 1
};

} // namespace fm

Q_DECLARE_METATYPE(fm::PanelId)

#endif // FM_PANEL_PANEL_ID_H
