#include "error_dialog.h"

#include <QMessageBox>

namespace fm {

void ErrorDialog::show(QWidget *parent, const QString &title, const QString &message) {
    QMessageBox::critical(parent, title, message);
}

void ErrorDialog::show(QWidget *parent, const QString &message) {
    QMessageBox::critical(parent, ErrorDialog::tr("Error"), message);
}

} // namespace fm
