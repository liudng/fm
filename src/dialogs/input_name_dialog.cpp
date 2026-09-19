#include "input_name_dialog.h"

#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
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

    // 初始宽度按初始名称长度自适应：文本渲染宽 + 控件边距
    // 下限为布局默认 sizeHint（短名观感不变），上限为主窗口宽度的 ~90%
    {
        const QFontMetrics fm = edit_->fontMetrics();
        // QLineEdit 默认 sizeHint 为 17 个 'x' 宽 + 框架边距，由此反推控件自身边距
        const int editChrome = edit_->sizeHint().width() - fm.horizontalAdvance(QLatin1Char('x')) * 17;
        const int margins = layout->contentsMargins().left() + layout->contentsMargins().right();
        const int wanted = editChrome + fm.horizontalAdvance(defaultName) + margins;

        const int minWidth = sizeHint().width();
        int refWidth = 0;
        if (const QWidget *pw = parentWidget()) {
            refWidth = pw->window()->width();
        } else {
            refWidth = QGuiApplication::primaryScreen()->availableGeometry().width();
        }
        const int maxWidth = qMax(qRound(refWidth * 0.9), minWidth);

        resize(qBound(minWidth, wanted, maxWidth), sizeHint().height());
    }
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
