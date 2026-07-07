#include "fm_application.h"

#include "../core/config_manager.h"
#include "../filelist/file_item.h"
#include "../ui/main_window.h"

#include <QDir>
#include <QLibraryInfo>

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
    // 1. 配置文件
    auto *cfg = ConfigManager::instance();
    cfg->ensureDefaultConfig();

    // 2. 默认主题
    const QString theme = cfg->value(QStringLiteral("UI"),
                                      QStringLiteral("theme"),
                                      QStringLiteral("Fusion")).toString();
    if (!theme.isEmpty()) {
        setStyle(theme);
    }

    // 3. 翻译
    const QString lang = cfg->value(QStringLiteral("UI"),
                                     QStringLiteral("language"),
                                     QStringLiteral("en")).toString();
    loadTranslation(lang);

    // 4. 主窗口
    mainWindow_ = new MainWindow();
    mainWindow_->show();
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
