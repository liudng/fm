#include "input_name_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace fm {

InputNameDialog::InputNameDialog(const QString &title, const QString &label,
                                 const QString &defaultName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *labelWidget = new QLabel(label, this);
    layout->addWidget(labelWidget);

    edit_ = new QLineEdit(defaultName, this);
    edit_->selectAll();
    layout->addWidget(edit_);

    hintLabel_ = new QLabel(this);
    hintLabel_->setStyleSheet(QStringLiteral("color: red;"));
    hintLabel_->hide();
    layout->addWidget(hintLabel_);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        validate();
        if (hintLabel_->isHidden()) accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(edit_, &QLineEdit::textChanged, this, &InputNameDialog::validate);
}

QString InputNameDialog::name() const
{
    return edit_->text();
}

void InputNameDialog::setExistingNames(const QStringList &names)
{
    existingNames_ = names;
    validate();
}

void InputNameDialog::validate()
{
    const QString n = edit_->text();
    if (n.isEmpty()) {
        hintLabel_->setText(tr("Name cannot be empty."));
        hintLabel_->show();
        return;
    }
    if (n.contains(QLatin1Char('/')) || n.contains(QLatin1Char('\\'))) {
        hintLabel_->setText(tr("Name cannot contain '/' or '\\'."));
        hintLabel_->show();
        return;
    }
    if (existingNames_.contains(n, Qt::CaseSensitive)) {
        hintLabel_->setText(tr("Name already exists. Please choose another."));
        hintLabel_->show();
        return;
    }
    hintLabel_->hide();
}

} // namespace fm
