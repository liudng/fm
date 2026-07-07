#include "properties_dialog.h"

#include <QDateTime>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

namespace fm {

namespace {
QString permsToString(QFile::Permissions p) {
    QString s = QStringLiteral("----------");
    if (p & QFile::ReadOwner)  s[0] = 'r';
    if (p & QFile::WriteOwner) s[1] = 'w';
    if (p & QFile::ExeOwner)   s[2] = 'x';
    if (p & QFile::ReadGroup)  s[3] = 'r';
    if (p & QFile::WriteGroup) s[4] = 'w';
    if (p & QFile::ExeGroup)   s[5] = 'x';
    if (p & QFile::ReadOther)  s[6] = 'r';
    if (p & QFile::WriteOther) s[7] = 'w';
    if (p & QFile::ExeOther)   s[8] = 'x';
    return s;
}

QWidget *makeRow(const QString &label, const QString &value) {
    auto *w = new QWidget;
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    auto *lab = new QLabel(label + QStringLiteral(":"));
    lab->setStyleSheet(QStringLiteral("font-weight: bold;"));
    l->addWidget(lab);
    auto *val = new QLabel(value);
    val->setWordWrap(true);
    val->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(val, 1);
    return w;
}
} // namespace

PropertiesDialog::PropertiesDialog(const FileItem &item, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Properties"));
    setMinimumWidth(500);
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    // 文件图标 + 名称（顶部）
    auto *headerLayout = new QHBoxLayout();
    auto *iconLabel = new QLabel(this);
    const QString iconName = item.isDir ? QStringLiteral("folder") : QStringLiteral("text-x-generic");
    iconLabel->setPixmap(QIcon::fromTheme(iconName).pixmap(48, 48));
    headerLayout->addWidget(iconLabel);
    auto *nameLabel = new QLabel(QStringLiteral("<b>%1</b>").arg(item.name), this);
    nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(nameLabel, 1);
    layout->addLayout(headerLayout);

    // 基本信息
    auto *basicBox = new QGroupBox(tr("Basic"), this);
    auto *basicLayout = new QVBoxLayout(basicBox);
    basicLayout->addWidget(makeRow(tr("Name"), item.name));
    basicLayout->addWidget(makeRow(tr("Path"), item.absolutePath));
    basicLayout->addWidget(makeRow(tr("Type"), item.mimeTypeComment));
    basicLayout->addWidget(makeRow(tr("MIME"), item.mimeTypeName));
    basicLayout->addWidget(makeRow(tr("Size"),
        item.isDir ? tr("(folder)") : QLocale().formattedDataSize(item.size)));
    layout->addWidget(basicBox);

    // 用户与权限
    auto *permBox = new QGroupBox(tr("User & Permissions"), this);
    auto *permLayout = new QVBoxLayout(permBox);
    permLayout->addWidget(makeRow(tr("Owner"), item.owner));
    permLayout->addWidget(makeRow(tr("Group"), item.group));
    permLayout->addWidget(makeRow(tr("Permissions"), permsToString(item.permissions)));
    layout->addWidget(permBox);

    // 时间戳
    auto *timeBox = new QGroupBox(tr("Timestamps"), this);
    auto *timeLayout = new QVBoxLayout(timeBox);
    timeLayout->addWidget(makeRow(tr("Created"),
        QLocale().toString(item.created, QLocale::LongFormat)));
    timeLayout->addWidget(makeRow(tr("Modified"),
        QLocale().toString(item.modified, QLocale::LongFormat)));
    layout->addWidget(timeBox);

    // inode 等系统信息
    auto *sysBox = new QGroupBox(tr("System Info"), this);
    auto *sysLayout = new QVBoxLayout(sysBox);
    sysLayout->addWidget(makeRow(tr("Inode"), QString::number(item.inode)));
    if (item.isSymLink) {
        sysLayout->addWidget(makeRow(tr("SymLink target"), item.symLinkTarget));
    }
    layout->addWidget(sysBox);

    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
}

} // namespace fm
