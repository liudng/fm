#include "properties_dialog.h"

#include "../core/config_manager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QProcess>
#include <QPushButton>
#include <QObject>
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

// 读取配置的日期时间格式
QString dtFormat() {
    return ConfigManager::instance()->value(
        QStringLiteral("File_Browser"), QStringLiteral("dateTimeFormat"),
        QStringLiteral("yyyy-MM-dd HH:mm")).toString();
}

QString formatDateTime(const QDateTime &dt) {
    if (!dt.isValid()) return QCoreApplication::translate("fm::PropertiesDialog", "(unknown)");
    const QString fmt = dtFormat();
    return fmt.isEmpty() ? dt.toString(Qt::ISODate) : dt.toString(fmt);
}

// 获取 ACL（getfacl），去掉注释行
QString getAcl(const QString &path) {
    QProcess p;
    p.start(QStringLiteral("getfacl"), {QStringLiteral("-cp"), path});
    if (!p.waitForFinished(3000)) return {};
    if (p.exitCode() != 0) return {};
    // 去除注释行（# 开头）和空行
    QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    QStringList lines;
    for (const QString &l : out.split(QLatin1Char('\n'))) {
        const QString t = l.trimmed();
        if (t.isEmpty() || t.startsWith(QLatin1Char('#'))) continue;
        lines << t;
    }
    return lines.join(QLatin1Char('\n'));
}

// 获取 ext2/ext3/ext4/btrfs/xfs 标志位（lsattr）
QString getExtFlags(const QString &path) {
    QProcess p;
    p.start(QStringLiteral("lsattr"), {QStringLiteral("-d"), path});
    if (!p.waitForFinished(3000)) return {};
    if (p.exitCode() != 0) return {};
    const QString out = QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
    // 输出形如 "-------------e-- /path/to/file"，取标志部分
    const int sp = out.indexOf(QLatin1Char(' '));
    return sp > 0 ? out.left(sp) : out;
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
    setMinimumWidth(750);  // 较原 500 增加约 1/2
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

    // === 基本信息 ===
    auto *basicBox = new QGroupBox(tr("Basic Information"), this);
    auto *basicLayout = new QVBoxLayout(basicBox);
    basicLayout->addWidget(makeRow(tr("Name"), item.name));
    basicLayout->addWidget(makeRow(tr("Path"), item.absolutePath));
    basicLayout->addWidget(makeRow(tr("Type"), item.mimeTypeComment));
    basicLayout->addWidget(makeRow(tr("MIME"), item.mimeTypeName));
    basicLayout->addWidget(makeRow(tr("Size"),
        item.isDir ? tr("(folder)") : QLocale().formattedDataSize(item.size)));
    basicLayout->addWidget(makeRow(tr("Disk Usage"),
        item.isDir ? tr("(folder)") : QLocale().formattedDataSize(item.diskUsage)));
    basicLayout->addWidget(makeRow(tr("Created"), formatDateTime(item.created)));
    basicLayout->addWidget(makeRow(tr("Modified"), formatDateTime(item.modified)));
    basicLayout->addWidget(makeRow(tr("Accessed"), formatDateTime(item.accessed)));
    layout->addWidget(basicBox);

    // === 用户与权限 ===
    auto *permBox = new QGroupBox(tr("User & Permissions"), this);
    auto *permLayout = new QVBoxLayout(permBox);
    permLayout->addWidget(makeRow(tr("Owner UID"), QString::number(item.ownerId)));
    permLayout->addWidget(makeRow(tr("Owner"), item.owner));
    permLayout->addWidget(makeRow(tr("Group GID"), QString::number(item.groupId)));
    permLayout->addWidget(makeRow(tr("Group"), item.group));
    permLayout->addWidget(makeRow(tr("Permissions"), permsToString(item.permissions)));
    // ACL 访问控制列表
    const QString acl = getAcl(item.absolutePath);
    permLayout->addWidget(makeRow(tr("ACL"),
        acl.isEmpty() ? tr("(none or unavailable)") : acl));
    permLayout->addWidget(makeRow(tr("Status Changed"), formatDateTime(item.statusChanged)));
    layout->addWidget(permBox);

    // === 系统信息 ===
    auto *sysBox = new QGroupBox(tr("System Info"), this);
    auto *sysLayout = new QVBoxLayout(sysBox);
    sysLayout->addWidget(makeRow(tr("Inode"), QString::number(item.inode)));
    // ext2/ext3/ext4/btrfs/xfs 标志位
    const QString flags = getExtFlags(item.absolutePath);
    sysLayout->addWidget(makeRow(tr("Filesystem Flags"),
        flags.isEmpty() ? tr("(unavailable)") : flags));
    if (item.isSymLink) {
        sysLayout->addWidget(makeRow(tr("Symlink Target"), item.symLinkTarget));
    }
    layout->addWidget(sysBox);

    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
}

} // namespace fm
