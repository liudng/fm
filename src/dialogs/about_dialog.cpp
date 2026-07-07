#include "about_dialog.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace fm {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("About fm"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *label = new QLabel(this);
    label->setText(tr(
        "<h2>fm</h2>"
        "<p>Linux dual-panel file manager</p>"
        "<p><b>Version:</b> 1.0.0</p>"
        "<p><b>Author:</b> fm team</p>"
        "<p>Copyright © 2026 fm team</p>"
        "<p>Licensed under GPL-3.0-or-later</p>"
    ));
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    auto *closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);
}

} // namespace fm
