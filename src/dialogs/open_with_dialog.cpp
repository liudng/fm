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

} // namespace

OpenWithDialog::OpenWithDialog(const QString &mimeType, const QString &fileName, QWidget *parent)
    : QDialog(parent), mimeType_(mimeType) {
    setWindowTitle(tr("Open With"));
    setMinimumWidth(500);
    setModal(true);

    auto *layout = new QVBoxLayout(this);

    auto *label = new QLabel(tr("Select an application to open \"%1\":").arg(fileName), this);
    layout->addWidget(label);

    appList_ = new QListWidget(this);
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
    // 搜索路径
    const QStringList dirs = {
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/applications"),
        QStringLiteral("/usr/share/applications"),
        QStringLiteral("/usr/local/share/applications"),
    };

    QSet<QString> seen;
    for (const QString &dirPath : dirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        const QStringList files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            const QString path = dir.filePath(file);
            if (seen.contains(path)) continue;
            if (!desktopSupportsMime(path, mimeType)) continue;
            seen.insert(path);
            DesktopApp app = parseDesktopFile(path);
            auto *item = new QListWidgetItem(QIcon::fromTheme(app.icon, QIcon::fromTheme(QStringLiteral("application-x-executable"))),
                                              app.name, appList_);
            item->setData(Qt::UserRole, path);
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
