#include "progress_dialog.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimerEvent>
#include <QVBoxLayout>

namespace fm {

ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent) {
    setModal(true);
    setWindowTitle(tr("Working..."));
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    titleLabel_ = new QLabel(this);
    layout->addWidget(titleLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    layout->addWidget(progressBar_);

    fileLabel_ = new QLabel(this);
    fileLabel_->setWordWrap(true);
    layout->addWidget(fileLabel_);

    cancelBtn_ = new QPushButton(tr("Cancel"), this);
    connect(cancelBtn_, &QPushButton::clicked, this, [this]() {
        canceled_ = true;
        emit canceled();
    });
    layout->addWidget(cancelBtn_, 0, Qt::AlignRight);

    // 初始隐藏，等待 showDelayed
    progressBar_->setValue(0);
}

void ProgressDialog::setOperationTitle(const QString &title) {
    titleLabel_->setText(title);
    setWindowTitle(title);
}

void ProgressDialog::setProgress(int percent) {
    if (percent < 0) {
        progressBar_->setRange(0, 0);  // 不确定模式
    } else {
        progressBar_->setRange(0, 100);
        progressBar_->setValue(percent);
    }
}

void ProgressDialog::setCurrentFile(const QString &fileName) {
    fileLabel_->setText(fileName.isEmpty() ? QString() : tr("Current: %1").arg(fileName));
}

void ProgressDialog::showDelayed() {
    showTimerId_ = startTimer(1000);  // 1s 后显示
}

void ProgressDialog::timerEvent(QTimerEvent *event) {
    if (event->timerId() == showTimerId_) {
        killTimer(showTimerId_);
        showTimerId_ = 0;
        if (!canceled_ && !isVisible()) {
            show();
        }
    }
}

} // namespace fm
