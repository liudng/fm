#ifndef FM_DIALOGS_ABOUT_DIALOG_H
#define FM_DIALOGS_ABOUT_DIALOG_H

#include <QDialog>

namespace fm {

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

} // namespace fm

#endif // FM_DIALOGS_ABOUT_DIALOG_H
