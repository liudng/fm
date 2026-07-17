#include "about_dialog.h"

#include "version.h"

#include <QApplication>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace fm {

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("About fm"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    // 应用图标
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QApplication::windowIcon().pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    auto *label = new QLabel(this);
    label->setText(
        tr("<h2>fm</h2>"
           "<p>Linux dual-panel file manager</p>"
           "<p><b>Version:</b> %1</p>"
           "<p><b>Author:</b> liudng</p>"
           "<p>Copyright © 2026 liudng</p>"
           "<p>Licensed under GPL-3.0-or-later</p>"
           "<p><a href=\"https://github.com/liudng/fm\">https://github.com/liudng/fm</a></p>")
            .arg(QString::fromLatin1(FM_VERSION)));
    label->setAlignment(Qt::AlignCenter);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    label->setOpenExternalLinks(true);
    layout->addWidget(label);

    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);
}

} // namespace fm
