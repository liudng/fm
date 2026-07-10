#ifndef FM_CORE_SHORTCUT_MANAGER_H
#define FM_CORE_SHORTCUT_MANAGER_H

#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QList>
#include <QMap>

class QAction;

namespace fm {

// 快捷键项
struct ShortcutItem
{
    QString id;              // 如 "file.refresh"
    QString defaultKey;      // 默认快捷键（字符串形式，如 "Ctrl+R"）
    QString currentKey;      // 当前用户配置
    bool conflicted = false; // 是否冲突
    QString displayText;     // 在设置对话框中显示的标题
};

// 快捷键管理（单例）
// - 维护所有可配置快捷键的映射
// - 应用到 QAction
// - 检测冲突（仅第一个生效）
// - 持久化到 [Shortcuts] section
class ShortcutManager : public QObject
{
    Q_OBJECT
public:
    static ShortcutManager *instance();

    // 初始化默认快捷键表（包含菜单/右键/键盘导航所有项）
    void initialize();

    // 获取所有快捷键项
    QList<ShortcutItem> allShortcuts() const;
    // 按 id 查询
    ShortcutItem shortcut(const QString &id) const;
    // 获取 id 对应的当前快捷键（若无则返回默认）
    QString shortcutFor(const QString &id) const;

    // 设置快捷键，返回是否成功（失败通常因冲突，但仍保存）
    bool setShortcut(const QString &id, const QKeySequence &seq);

    // 绑定到 QAction：自动应用并跟踪
    void applyToAction(QAction *action, const QString &id);

    // 检测冲突：仅第一个设置的快捷键生效，其余标记 conflicted=true
    void detectConflicts();

    // 从配置加载并保存到配置
    void loadFromConfig();
    void saveToConfig();

signals:
    void shortcutsChanged();

private:
    ShortcutManager(QObject *parent = nullptr);

    // 重新对所有已绑定 QAction 应用当前快捷键（设置变更后调用）
    void reapplyShortcuts();

    QMap<QString, ShortcutItem> items_;               // id -> item
    QMap<QString, QPointer<QAction>> actionBindings_; // id -> QAction（弱引用）
};

} // namespace fm

#endif // FM_CORE_SHORTCUT_MANAGER_H
