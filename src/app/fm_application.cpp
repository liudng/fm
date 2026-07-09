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
#include <QIcon>
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
    // 优先使用内置 fm.png 图标；未安装到系统图标主题时也能正常显示
    QIcon appIcon(QStringLiteral(":/fm.png"));
    if (appIcon.isNull()) {
        appIcon = QIcon::fromTheme(QStringLiteral("fm"));
    }
    setWindowIcon(appIcon);

    // 注册 metatype
    qRegisterMetaType<FileItem>("fm::FileItem");

    // 图标主题在 initialize() 中根据配置应用（构造函数时尚未加载配置）
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

    // 3. 默认主题（风格 + 图标主题）
    const QString theme = cfg->value(QStringLiteral("UI"),
                                      QStringLiteral("theme"),
                                      QStringLiteral("Fusion")).toString();
    if (!theme.isEmpty()) {
        setStyle(theme);
    }
    applyIconTheme();

    // 4. 翻译
    const QString lang = cfg->value(QStringLiteral("UI"),
                                     QStringLiteral("language"),
                                     QStringLiteral("en")).toString();
    loadTranslation(lang);

    // 5. 监听配置变更（风格/图标主题实时应用）
    connect(cfg, &ConfigManager::configChanged, this, [this](const QString &section) {
        if (section == QStringLiteral("UI")) {
            auto *c = ConfigManager::instance();
            const QString t = c->value(QStringLiteral("UI"), QStringLiteral("theme"),
                                         QStringLiteral("Fusion")).toString();
            if (t.isEmpty()) setStyle(QString());
            else setStyle(QStyleFactory::create(t));
            applyIconTheme();
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

    // Qt 自带翻译（qtbase 标准对话框等）
    QTranslator *qtTranslator = new QTranslator(this);
    if (qtTranslator->load(QStringLiteral("qtbase_") + language,
                           QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        installTranslator(qtTranslator);
    } else {
        delete qtTranslator;
    }

    // 应用翻译：优先从嵌入资源加载（:/i18n/），失败则从安装目录加载
    const QString qmFile = QStringLiteral("fm_") + language + QStringLiteral(".qm");
    bool loaded = translator_.load(qmFile, QStringLiteral(":/i18n"));
    if (!loaded) {
        // 尝试从安装目录加载
        const QString trPath = QDir(applicationDirPath()).absoluteFilePath(
            QStringLiteral("../share/fm/translations"));
        loaded = translator_.load(qmFile, trPath);
    }
    if (loaded) {
        installTranslator(&translator_);
    }
    // 加载失败时使用源码内 tr 默认值（英文）
}

void FmApplication::applyIconTheme() {
    // 从配置读取图标主题（UI/iconTheme）
    // 空值表示自动：优先使用 gnome-icon-theme（其包含完整的标准动作图标
    // .png 文件，例如 go-previous/document-new/edit-cut 等），避免某些现代
    // 主题（如 Adwaita）仅提供 *-symbolic.svg 导致部分 QAction 图标无法显示。
    // 仅当对应主题确实可用时才切换。
    auto *cfg = ConfigManager::instance();
    const QString iconTheme = cfg->value(QStringLiteral("UI"),
                                           QStringLiteral("iconTheme"),
                                           QString()).toString();
    if (!iconTheme.isEmpty()) {
        QIcon::setThemeName(iconTheme);
        return;
    }
    // 自动：gnome 可用则使用 gnome，否则保持默认（hicolor）
    if (QDir(QStringLiteral("/usr/share/icons/gnome")).exists() ||
        QDir(QStringLiteral("/usr/local/share/icons/gnome")).exists()) {
        QIcon::setThemeName(QStringLiteral("gnome"));
    }
}

} // namespace fm
