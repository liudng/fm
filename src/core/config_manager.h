#ifndef FM_CORE_CONFIG_MANAGER_H
#define FM_CORE_CONFIG_MANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>

namespace fm {

// 配置文件管理（单例）
// 路径：~/.config/fm/config.ini
// Phase 1 仅实现最小化读写与默认配置创建；损坏处理在后续阶段补全
class ConfigManager : public QObject {
    Q_OBJECT
public:
    static ConfigManager *instance();

    // 通用读写
    QVariant value(const QString &section, const QString &key,
                   const QVariant &defaultValue = {}) const;
    void setValue(const QString &section, const QString &key,
                  const QVariant &value);
    bool contains(const QString &section, const QString &key) const;

    // 配置文件路径
    QString filePath() const;

    // 首次启动时创建默认配置
    void ensureDefaultConfig();

signals:
    void configChanged(const QString &section);

private:
    ConfigManager(QObject *parent = nullptr);

    // 拼接完整 key：section + '/' + key
    static QString fullKey(const QString &section, const QString &key);

    QSettings *settings_;
};

} // namespace fm

#endif // FM_CORE_CONFIG_MANAGER_H
