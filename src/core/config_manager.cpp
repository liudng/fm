#include "config_manager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace fm {

ConfigManager *ConfigManager::instance() {
    static ConfigManager inst;
    return &inst;
}

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent) {
    // 配置目录 ~/.config/fm
    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
        QDir::separator() + QStringLiteral("fm");
    QDir().mkpath(configDir);

    const QString configPath = configDir + QDir::separator() + QStringLiteral("config.ini");
    // Qt6 中 QSettings 默认使用 UTF-8 编码，无需 setIniCodec
    settings_ = new QSettings(configPath, QSettings::IniFormat, this);
}

QString ConfigManager::filePath() const {
    return settings_->fileName();
}

bool ConfigManager::load() {
    settings_->sync();
    const QSettings::Status s = settings_->status();
    loaded_ = (s == QSettings::NoError);
    return loaded_;
}

bool ConfigManager::rebuild() {
    // 备份原文件
    backupCurrentFile();
    // 删除原文件
    QFile::remove(filePath());
    // 重新创建 settings 对象（重置内存状态）
    delete settings_;
    settings_ = new QSettings(filePath(), QSettings::IniFormat, this);
    ensureDefaultConfig();
    loaded_ = true;
    return true;
}

void ConfigManager::saveFailureRecover() {
    // 备份原文件（可能已损坏）
    backupCurrentFile();
    // 删除原文件，用当前内存配置重新写入
    QFile::remove(filePath());
    settings_->sync();
    loaded_ = (settings_->status() == QSettings::NoError);
}

QSettings::Status ConfigManager::status() const {
    return settings_->status();
}

bool ConfigManager::backupCurrentFile(QString *backupPath) {
    const QString src = filePath();
    if (!QFileInfo::exists(src)) return false;

    // 时间戳格式：yyyyMMdd_HHmmss_zzz
    const QString ts = QDateTime::currentDateTime().toString(
        QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString dst = src + QStringLiteral(".") + ts + QStringLiteral(".bak");
    if (QFile::copy(src, dst)) {
        if (backupPath) *backupPath = dst;
        return true;
    }
    return false;
}

void ConfigManager::ensureDefaultConfig() {
    // 仅当文件不存在或为空时写入默认值
    if (QFileInfo::exists(filePath()) && !settings_->allKeys().isEmpty()) {
        return;
    }

    // [UI]
    setValue(QStringLiteral("UI"), QStringLiteral("language"), QStringLiteral("en"));
    setValue(QStringLiteral("UI"), QStringLiteral("theme"), QStringLiteral("Fusion"));

    // [Panels]
    setValue(QStringLiteral("Panels"), QStringLiteral("orientation"),
             static_cast<int>(Qt::Horizontal));  // 左右
    setValue(QStringLiteral("Panels"), QStringLiteral("panel1Visible"), true);
    setValue(QStringLiteral("Panels"), QStringLiteral("panel2Visible"), true);

    // [File_Browser]
    setValue(QStringLiteral("File_Browser"), QStringLiteral("showHidden"), false);
    setValue(QStringLiteral("File_Browser"), QStringLiteral("dateTimeFormat"),
             QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    // [File_Browser_Columns] - 默认四列可见
    setValue(QStringLiteral("File_Browser_Columns"), QStringLiteral("columns"),
             QStringLiteral("Icon,Name,Size,Modified"));
    // 列宽（像素）；Name 列不存储（Stretch 模式自动填充）
    setValue(QStringLiteral("File_Browser_Columns"), QStringLiteral("width_Icon"), 28);
    setValue(QStringLiteral("File_Browser_Columns"), QStringLiteral("width_Size"), 80);
    setValue(QStringLiteral("File_Browser_Columns"), QStringLiteral("width_Modified"), 140);

    // [Shortcuts] - 默认快捷键（由 ShortcutManager 提供并写入）
    // [OpenWith] - 空，按需追加
    settings_->sync();
}

QString ConfigManager::fullKey(const QString &section, const QString &key) {
    return section + QLatin1Char('/') + key;
}

QVariant ConfigManager::value(const QString &section, const QString &key,
                              const QVariant &defaultValue) const {
    return settings_->value(fullKey(section, key), defaultValue);
}

void ConfigManager::setValue(const QString &section, const QString &key,
                            const QVariant &value) {
    settings_->setValue(fullKey(section, key), value);
    settings_->sync();
    emit configChanged(section);
}

bool ConfigManager::contains(const QString &section, const QString &key) const {
    return settings_->contains(fullKey(section, key));
}

} // namespace fm
