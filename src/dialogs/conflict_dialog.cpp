#include "conflict_dialog.h"

#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace fm {

ConflictDialog::ConflictDialog(const QString &sourceName, const QString &destPath,
                               bool allowBatch, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("File Conflict"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *label = new QLabel(this);
    const QFileInfo destInfo(destPath);
    label->setText(tr("A file named \"%1\" already exists in \"%2\".\n\n"
                      "Source: %3\n"
                      "Would you like to overwrite it?")
                       .arg(destInfo.fileName())
                       .arg(destInfo.absolutePath())
                       .arg(sourceName));
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *btnLayout = new QHBoxLayout();
    layout->addLayout(btnLayout);

    auto *overwriteBtn = new QPushButton(tr("Overwrite"), this);
    auto *skipBtn = new QPushButton(tr("Skip"), this);
    auto *renameBtn = new QPushButton(tr("Rename"), this);
    connect(overwriteBtn, &QPushButton::clicked, this, &ConflictDialog::chooseOverwrite);
    connect(skipBtn, &QPushButton::clicked, this, &ConflictDialog::chooseSkip);
    connect(renameBtn, &QPushButton::clicked, this, &ConflictDialog::chooseRename);
    btnLayout->addWidget(overwriteBtn);
    btnLayout->addWidget(skipBtn);
    btnLayout->addWidget(renameBtn);

    if (allowBatch) {
        auto *overwriteAllBtn = new QPushButton(tr("Overwrite All"), this);
        auto *skipAllBtn = new QPushButton(tr("Skip All"), this);
        auto *renameAllBtn = new QPushButton(tr("Rename All"), this);
        connect(overwriteAllBtn, &QPushButton::clicked, this, &ConflictDialog::chooseOverwriteAll);
        connect(skipAllBtn, &QPushButton::clicked, this, &ConflictDialog::chooseSkipAll);
        connect(renameAllBtn, &QPushButton::clicked, this, &ConflictDialog::chooseRenameAll);
        btnLayout->addWidget(overwriteAllBtn);
        btnLayout->addWidget(skipAllBtn);
        btnLayout->addWidget(renameAllBtn);
    }

    auto *cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
}

void ConflictDialog::chooseOverwrite() {
    resolution_ = ConflictResolution::Overwrite;
    accept();
}
void ConflictDialog::chooseSkip() {
    resolution_ = ConflictResolution::Skip;
    accept();
}
void ConflictDialog::chooseRename() {
    resolution_ = ConflictResolution::Rename;
    accept();
}
void ConflictDialog::chooseOverwriteAll() {
    resolution_ = ConflictResolution::OverwriteAll;
    accept();
}
void ConflictDialog::chooseSkipAll() {
    resolution_ = ConflictResolution::SkipAll;
    accept();
}
void ConflictDialog::chooseRenameAll() {
    resolution_ = ConflictResolution::RenameAll;
    accept();
}

} // namespace fm
