#include "settings_dialog.h"

#include "isettings_page.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fm {

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(800, 500);

    auto *mainLayout = new QVBoxLayout(this);

    auto *hbox = new QHBoxLayout;
    mainLayout->addLayout(hbox, 1);

    // 左侧 sidebar（占 1/5 宽度）
    sidebar_ = new QListWidget(this);
    sidebar_->setMaximumWidth(180);
    hbox->addWidget(sidebar_);

    // 右侧内容区
    contentStack_ = new QStackedWidget(this);
    hbox->addWidget(contentStack_, 1);

    // 底部按钮
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    mainLayout->addWidget(buttonBox);

    applyBtn_ = buttonBox->button(QDialogButtonBox::Apply);

    connect(sidebar_, &QListWidget::currentRowChanged, this, &SettingsDialog::onPageChanged);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onOk);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::onCancel);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &SettingsDialog::onApply);
}

void SettingsDialog::addPage(ISettingsPage *page)
{
    pages_.append(page);
    sidebar_->addItem(page->title());
    contentStack_->addWidget(page->widget());
    page->load();
}

void SettingsDialog::showPage(const QString &pageId)
{
    for (int i = 0; i < pages_.size(); ++i) {
        if (pages_.at(i)->id() == pageId) {
            sidebar_->setCurrentRow(i);
            break;
        }
    }
}

void SettingsDialog::onPageChanged(int index)
{
    if (index < 0 || index >= contentStack_->count()) return;
    contentStack_->setCurrentIndex(index);
}

void SettingsDialog::onApply()
{
    for (ISettingsPage *p : pages_) {
        p->apply();
    }
}

void SettingsDialog::onOk()
{
    onApply();
    accept();
}

void SettingsDialog::onCancel()
{
    // 恢复到上次 apply 的状态
    for (ISettingsPage *p : pages_) {
        p->reset();
    }
    reject();
}

} // namespace fm
