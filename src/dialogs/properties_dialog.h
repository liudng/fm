#ifndef FM_DIALOGS_PROPERTIES_DIALOG_H
#define FM_DIALOGS_PROPERTIES_DIALOG_H

#include "../filelist/file_item.h"

#include <QDialog>

namespace fm {

class PropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PropertiesDialog(const FileItem &item, QWidget *parent = nullptr);
};

} // namespace fm

#endif // FM_DIALOGS_PROPERTIES_DIALOG_H
