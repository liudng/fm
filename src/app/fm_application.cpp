#include "fm_application.h"

#include "../app/single_instance.h"
#include "../core/column_manager.h"
#include "../core/config_manager.h"
#include "../core/shortcut_manager.h"
#include "../filelist/file_item.h"
#include "../ui/main_window.h"

#include <QAbstractButton>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QStyleFactory>

namespace fm {

FmApplication::FmApplication(int &argc, char **argv)
    : QApplication(argc, argv) {
    setApplicationName(QStringLiteral("fm"));
    setApplicationVersion(QStringLiteral("1.0.0"));
    setOrganizationName(QStringLiteral("fm"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("system-file-manager")));

    // 注册 metatype
    qRegisterMetaType<FileItem>("fm::FileItem");
}

bool FmApplication::initialize() {
    // 0. 单实例检测
    singleInstance_ = new SingleInstance(this);
    if (!singleInstance_->tryLock()) {
        // 已有实例在运行：本进程不需要继续初始化
        // main.cpp 会在 initialize 返回后处理 sendPaths
        return false;
    }

    // 1. 配置文件加载与损坏检测
    auto *cfg = ConfigManager::instance();
    if (!cfg->load()) {
        // 配置文件损坏：提示用户选择重建或退出
        QMessageBox box(QMessageBox::Warning, tr("Configuration Error"),
                        tr("The configuration file is corrupted or cannot be read."),
                        QMessageBox::NoButton, nullptr);
        QPushButton *rebuildBtn = box.addButton(tr("Rebuild"), QMessageBox::AcceptRole);
        box.addButton(tr("Exit"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() != rebuildBtn) {
            return false;
        }
        cfg->rebuild();
    } else {
        cfg->ensureDefaultConfig();
    }

    // 2. 加载快捷键、列配置
    ShortcutManager::instance()->initialize();
    ColumnManager::instance()->loadFromConfig();

    // 3. 默认主题
    const QString theme = cfg->value(QStringLiteral("UI"),
                                      QStringLiteral("theme"),
                                      QStringLiteral("Fusion")).toString();
    if (!theme.isEmpty()) {
        setStyle(theme);
    }

    // 4. 翻译
    const QString lang = cfg->value(QStringLiteral("UI"),
                                     QStringLiteral("language"),
                                     QStringLiteral("en")).toString();
    loadTranslation(lang);

    // 5. 监听配置变更（语言/主题实时应用）
    connect(cfg, &ConfigManager::configChanged, this, [this](const QString &section) {
        if (section == QStringLiteral("UI")) {
            auto *c = ConfigManager::instance();
            const QString t = c->value(QStringLiteral("UI"), QStringLiteral("theme"),
                                         QStringLiteral("Fusion")).toString();
            if (t.isEmpty()) setStyle(QString());
            else setStyle(QStyleFactory::create(t));
        }
    });

    // 6. 主窗口
    mainWindow_ = new MainWindow();
    mainWindow_->show();

    // 7. 监听单实例路径接收
    connect(singleInstance_, &SingleInstance::pathsReceived,
            mainWindow_, &MainWindow::addPathsToPanels);

    return true;
}

void FmApplication::loadTranslation(const QString &language) {
    removeTranslator(&translator_);

    // Qt 自带翻译
    QTranslator *qtTranslator = new QTranslator(this);
    if (qtTranslator->load(QStringLiteral("qtbase_") + language,
                           QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        installTranslator(qtTranslator);
    } else {
        delete qtTranslator;
    }

    // 应用翻译（后续阶段补全 .ts 文件）
    // 此处先尝试加载，失败则使用源码内 tr 默认值（英文）
    QString trPath = QDir(applicationDirPath()).absoluteFilePath(QStringLiteral("../share/fm/translations"));
    if (translator_.load(QStringLiteral("fm_") + language, trPath) ||
        translator_.load(QStringLiteral("fm_") + language,
                        QStringLiteral(":/translations"))) {
        installTranslator(&translator_);
    }
}

} // namespace fm
