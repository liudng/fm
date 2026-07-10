#ifndef FM_CORE_CONFIG_MANAGER_H
#define FM_CORE_CONFIG_MANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>

namespace fm {

// 配置文件管理（单例）
// 路径：~/.config/fm/config.ini
// 提供损坏检测与备份重建
class ConfigManager : public QObject
{
    Q_OBJECT
public:
    static ConfigManager *instance();

    // 通用读写
    QVariant value(const QString &section, const QString &key,
                   const QVariant &defaultValue = {}) const;
    void setValue(const QString &section, const QString &key, const QVariant &value);
    bool contains(const QString &section, const QString &key) const;

    // 配置文件路径
    QString filePath() const;

    // 首次启动时创建默认配置
    void ensureDefaultConfig();

    // 损坏检测与重建：
    // - load() 返回 false 表示配置文件解析失败
    // - rebuild() 备份原文件并写入默认配置
    // - saveFailureRecover() 运行中保存失败时调用，备份原文件并写入当前内存配置
    bool load();
    bool rebuild();
    void saveFailureRecover();

    // 状态查询
    bool isLoaded() const { return loaded_; }
    QSettings::Status status() const;

signals:
    void configChanged(const QString &section);

private:
    ConfigManager(QObject *parent = nullptr);

    // 拼接完整 key：section + '/' + key
    static QString fullKey(const QString &section, const QString &key);

    // 备份当前文件（添加精确到毫秒的时间戳后缀）
    bool backupCurrentFile(QString *backupPath = nullptr);

    QSettings *settings_;
    bool loaded_ = false;
};

} // namespace fm

#endif // FM_CORE_CONFIG_MANAGER_H
