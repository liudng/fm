#include "open_with_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

namespace fm {

namespace {

struct DesktopApp {
    QString desktopFile;
    QString name;
    QString exec;
    QString icon;
    bool noDisplay = false;
    QString type;   // Application/Link/Directory
};

// 解析 .desktop 文件中的关键字段
DesktopApp parseDesktopFile(const QString &path) {
    DesktopApp app;
    app.desktopFile = path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return app;
    QTextStream ts(&f);
    QString line;
    bool inDesktopEntry = false;
    while (ts.readLineInto(&line)) {
        if (line.startsWith(QLatin1String("["))) {
            inDesktopEntry = (line == QStringLiteral("[Desktop Entry]"));
            continue;
        }
        if (!inDesktopEntry) continue;
        if (line.startsWith(QLatin1String("Name="))) {
            app.name = line.mid(5);
        } else if (line.startsWith(QLatin1String("Exec="))) {
            app.exec = line.mid(5);
        } else if (line.startsWith(QLatin1String("Icon="))) {
            app.icon = line.mid(5);
        } else if (line.startsWith(QLatin1String("NoDisplay="))) {
            app.noDisplay = (line.mid(10).trimmed().toLower() == QStringLiteral("true"));
        } else if (line.startsWith(QLatin1String("Type="))) {
            app.type = line.mid(5).trimmed();
        }
    }
    if (app.name.isEmpty()) {
        app.name = QFileInfo(path).completeBaseName();
    }
    return app;
}

// 检查 .desktop 是否声明了支持某 MIME 类型
bool desktopSupportsMime(const QString &path, const QString &mimeType) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    QString line;
    bool inDesktopEntry = false;
    while (ts.readLineInto(&line)) {
        if (line.startsWith(QLatin1String("["))) {
            inDesktopEntry = (line == QStringLiteral("[Desktop Entry]"));
            continue;
        }
        if (!inDesktopEntry) continue;
        if (line.startsWith(QLatin1String("MimeType="))) {
            const QStringList mimes = line.mid(9).split(QLatin1Char(';'), Qt::SkipEmptyParts);
            if (mimes.contains(mimeType)) return true;
        }
    }
    return false;
}

// 从 mimeapps.list 中解析与某 MIME 类型关联的 .desktop 文件名
// 检查 [Added Associations] 和 [Default Applications] 两个段
QStringList appsFromMimeapps(const QString &mimeType) {
    QStringList result;
    // mimeapps.list 标准位置：
    //   ~/.config/mimeapps.list（用户）
    //   $XDG_CONFIG_DIRS/mimeapps.list（系统，通常 /etc/xdg/mimeapps.list）
    //   $XDG_DATA_DIRS/applications/mimeapps.list（系统，旧标准）
    QStringList mimeappsPaths;
    const QStringList configDirs = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    for (const QString &d : configDirs) {
        mimeappsPaths << (d + QStringLiteral("/mimeapps.list"));
    }
    const QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &d : dataDirs) {
        mimeappsPaths << (d + QStringLiteral("/applications/mimeapps.list"));
    }

    for (const QString &mapPath : mimeappsPaths) {
        QFile f(mapPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream ts(&f);
        QString line;
        while (ts.readLineInto(&line)) {
            line = line.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq < 0) continue;
            if (line.left(eq).trimmed() != mimeType) continue;
            const QStringList entries = line.mid(eq + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
            for (const QString &entry : entries) {
                if (!result.contains(entry)) {
                    result << entry;
                }
            }
        }
    }
    return result;
}

// 收集所有应用搜索目录（XDG data dirs 下的 applications/）
QStringList applicationDirs() {
    QStringList dirs;
    const QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &d : dataDirs) {
        dirs << (d + QStringLiteral("/applications"));
    }
    dirs.removeDuplicates();
    return dirs;
}

// 在应用搜索目录中查找 .desktop 文件名对应的完整路径
QString resolveDesktopFile(const QString &desktopName, const QStringList &dirs) {
    for (const QString &dir : dirs) {
        const QString path = dir + QLatin1Char('/') + desktopName;
        if (QFile::exists(path)) return path;
    }
    return {};
}

} // namespace

OpenWithDialog::OpenWithDialog(const QString &mimeType, const QString &fileName, QWidget *parent)
    : QDialog(parent), mimeType_(mimeType) {
    setWindowTitle(tr("Open With"));
    setMinimumWidth(500);
    setMinimumHeight(450);
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *label = new QLabel(tr("Select an application to open \"%1\":").arg(fileName), this);
    layout->addWidget(label);

    appList_ = new QListWidget(this);
    appList_->setMinimumHeight(300);
    layout->addWidget(appList_);

    // 自定义命令
    auto *customLayout = new QHBoxLayout();
    auto *customLabel = new QLabel(tr("Custom command:"), this);
    customEdit_ = new QLineEdit(this);
    auto *browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, &QPushButton::clicked, this, &OpenWithDialog::onCustomBrowse);
    customLayout->addWidget(customLabel);
    customLayout->addWidget(customEdit_, 1);
    customLayout->addWidget(browseBtn);
    layout->addLayout(customLayout);

    rememberCheck_ = new QCheckBox(tr("Remember this choice for this file type"), this);
    layout->addWidget(rememberCheck_);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (appList_->currentItem()) {
            selectedDesktopFile_ = appList_->currentItem()->data(Qt::UserRole).toString();
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    populateApplications(mimeType);
    connect(appList_, &QListWidget::itemSelectionChanged, this, &OpenWithDialog::onSelectionChanged);
}

void OpenWithDialog::populateApplications(const QString &mimeType) {
    const QStringList dirs = applicationDirs();

    QSet<QString> seenPaths;

    // 辅助：添加 .desktop 文件到列表（跳过 NoDisplay=true 和非 Application 类型）
    auto addApp = [&](const QString &path) {
        if (seenPaths.contains(path)) return;
        seenPaths.insert(path);
        DesktopApp app = parseDesktopFile(path);
        // 跳过标记为 NoDisplay 或非 Application 类型的条目
        if (app.noDisplay) return;
        if (!app.type.isEmpty() && app.type != QStringLiteral("Application")) return;
        auto *item = new QListWidgetItem(
            QIcon::fromTheme(app.icon, QIcon::fromTheme(QStringLiteral("application-x-executable"))),
            app.name, appList_);
        item->setData(Qt::UserRole, path);
    };

    // 1. 先添加 mimeapps.list 中关联的应用（即使 .desktop 的 MimeType= 未声明）
    //    这覆盖了用户/系统通过 mimeapps.list 设置的默认应用和关联应用
    const QStringList associated = appsFromMimeapps(mimeType);
    for (const QString &desktopName : associated) {
        const QString path = resolveDesktopFile(desktopName, dirs);
        if (!path.isEmpty()) {
            addApp(path);
        }
    }

    // 2. 再添加 .desktop 文件中 MimeType= 声明支持该类型的应用
    for (const QString &dirPath : dirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        const QStringList files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            const QString path = dir.filePath(file);
            if (seenPaths.contains(path)) continue;
            if (!desktopSupportsMime(path, mimeType)) continue;
            addApp(path);
        }
    }

    // 3. 兜底：如果以上两步均未找到任何应用，列出所有 Application 类型的 .desktop 文件，
    //    以便用户至少能选择一个应用来打开文件
    if (appList_->count() == 0) {
        for (const QString &dirPath : dirs) {
            QDir dir(dirPath);
            if (!dir.exists()) continue;
            const QStringList files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
            for (const QString &file : files) {
                const QString path = dir.filePath(file);
                addApp(path);  // addApp 内部用 seenPaths 去重，并跳过 NoDisplay/非 Application 类型
            }
        }
    }
}

void OpenWithDialog::onSelectionChanged() {
    customEdit_->clear();
}

void OpenWithDialog::onCustomBrowse() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Select Application"),
                                                        QStringLiteral("/usr/bin"));
    if (!path.isEmpty()) {
        customEdit_->setText(path);
        appList_->clearSelection();
    }
}

QString OpenWithDialog::selectedApplication() const {
    if (!customEdit_->text().isEmpty()) return customEdit_->text();
    return selectedDesktopFile_;
}

bool OpenWithDialog::rememberChoice() const {
    return rememberCheck_->isChecked();
}

bool OpenWithDialog::isCustomCommand() const {
    return !customEdit_->text().isEmpty();
}

} // namespace fm
