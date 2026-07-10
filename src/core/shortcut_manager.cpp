#include "shortcut_manager.h"

#include "../core/config_manager.h"

#include <QAction>

namespace fm {

namespace {

// 默认快捷键定义表（id, defaultKey, displayText）
struct DefaultShortcut
{
    const char *id;
    const char *defaultKey;
    const char *displayText;
};

const DefaultShortcut kDefaultShortcuts[] = {
    // 文件菜单
    {"file.new_tab", "Ctrl+T", QT_TRANSLATE_NOOP("Shortcut", "New Tab")},
    {"file.close_tab", "Ctrl+W", QT_TRANSLATE_NOOP("Shortcut", "Close Tab")},
    {"file.clone_tab", "Ctrl+Shift+T", QT_TRANSLATE_NOOP("Shortcut", "Clone Tab")},
    {"file.new_file", "Ctrl+N", QT_TRANSLATE_NOOP("Shortcut", "New File")},
    {"file.new_folder", "F7", QT_TRANSLATE_NOOP("Shortcut", "New Folder")},
    {"file.quit", "", QT_TRANSLATE_NOOP("Shortcut", "Quit")},

    // 选项卡上下文菜单
    {"tab.close", "", QT_TRANSLATE_NOOP("Shortcut", "Close Tab (Context)")},
    {"tab.close_others", "", QT_TRANSLATE_NOOP("Shortcut", "Close Other Tabs")},
    {"tab.clone", "", QT_TRANSLATE_NOOP("Shortcut", "Clone Tab (Context)")},

    // 文件列表右键菜单（选中）
    {"filelist.open", "Return", QT_TRANSLATE_NOOP("Shortcut", "Open")},
    {"filelist.open_with", "Ctrl+Shift+O", QT_TRANSLATE_NOOP("Shortcut", "Open With...")},
    {"filelist.rename", "F2", QT_TRANSLATE_NOOP("Shortcut", "Rename")},
    {"filelist.cut", "Ctrl+X", QT_TRANSLATE_NOOP("Shortcut", "Cut")},
    {"filelist.copy", "Ctrl+C", QT_TRANSLATE_NOOP("Shortcut", "Copy")},
    {"filelist.cut_to_opposite", "F6", QT_TRANSLATE_NOOP("Shortcut", "Cut to Opposite")},
    {"filelist.copy_to_opposite", "F5", QT_TRANSLATE_NOOP("Shortcut", "Copy to Opposite")},
    {"filelist.copy_path", "", QT_TRANSLATE_NOOP("Shortcut", "Copy Path")},
    {"filelist.copy_name", "", QT_TRANSLATE_NOOP("Shortcut", "Copy File Name")},
    {"filelist.paste", "Ctrl+V", QT_TRANSLATE_NOOP("Shortcut", "Paste")},
    {"filelist.trash", "Delete", QT_TRANSLATE_NOOP("Shortcut", "Move to Trash")},
    {"filelist.delete", "Shift+Delete", QT_TRANSLATE_NOOP("Shortcut", "Delete Permanently")},
    {"filelist.properties", "Alt+Return", QT_TRANSLATE_NOOP("Shortcut", "Properties")},

    // 文件列表右键菜单（无选中）
    {"filelist.back", "Alt+Left", QT_TRANSLATE_NOOP("Shortcut", "Back")},
    {"filelist.forward", "Alt+Right", QT_TRANSLATE_NOOP("Shortcut", "Forward")},
    {"filelist.up", "Alt+Up", QT_TRANSLATE_NOOP("Shortcut", "Up")},
    {"filelist.refresh", "Ctrl+R", QT_TRANSLATE_NOOP("Shortcut", "Refresh")},

    // 设置菜单
    {"settings.switch_active_panel", "Tab", QT_TRANSLATE_NOOP("Shortcut", "Switch Active Panel")},
    {"settings.toggle_orientation", "", QT_TRANSLATE_NOOP("Shortcut", "Toggle Orientation")},
    {"settings.toggle_panel1", "", QT_TRANSLATE_NOOP("Shortcut", "Toggle Panel 1 Visible")},
    {"settings.toggle_panel2", "", QT_TRANSLATE_NOOP("Shortcut", "Toggle Panel 2 Visible")},
    {"settings.toggle_hidden", "Ctrl+H", QT_TRANSLATE_NOOP("Shortcut", "Show Hidden Files")},

    // 帮助菜单
    {"help.about", "", QT_TRANSLATE_NOOP("Shortcut", "About")},

    // 键盘导航
    {"nav.focus_panel", "Ctrl+Tab", QT_TRANSLATE_NOOP("Shortcut", "Next Tab")},
};

const int kDefaultShortcutCount = sizeof(kDefaultShortcuts) / sizeof(DefaultShortcut);

} // namespace

ShortcutManager *ShortcutManager::instance()
{
    static ShortcutManager inst;
    return &inst;
}

ShortcutManager::ShortcutManager(QObject *parent) : QObject(parent) {}

void ShortcutManager::initialize()
{
    // 1. 加载默认值
    for (int i = 0; i < kDefaultShortcutCount; ++i) {
        const auto &def = kDefaultShortcuts[i];
        ShortcutItem item;
        item.id = QString::fromLatin1(def.id);
        item.defaultKey = QString::fromLatin1(def.defaultKey);
        item.currentKey = item.defaultKey;
        item.displayText = QString::fromLatin1(def.displayText);
        items_.insert(item.id, item);
    }

    // 2. 从配置加载用户自定义
    loadFromConfig();

    // 3. 检测冲突
    detectConflicts();
}

QList<ShortcutItem> ShortcutManager::allShortcuts() const
{
    return items_.values();
}

ShortcutItem ShortcutManager::shortcut(const QString &id) const
{
    return items_.value(id);
}

QString ShortcutManager::shortcutFor(const QString &id) const
{
    auto it = items_.constFind(id);
    if (it == items_.constEnd()) return {};
    const auto &item = it.value();
    // 冲突项不返回快捷键（仅第一个生效）
    if (item.conflicted) return {};
    return item.currentKey.isEmpty() ? item.defaultKey : item.currentKey;
}

bool ShortcutManager::setShortcut(const QString &id, const QKeySequence &seq)
{
    auto it = items_.find(id);
    if (it == items_.end()) return false;
    it.value().currentKey = seq.toString();
    // 重新检测冲突
    detectConflicts();
    // 重新应用到所有已绑定的 QAction，使修改立即生效
    reapplyShortcuts();
    emit shortcutsChanged();
    return true;
}

void ShortcutManager::applyToAction(QAction *action, const QString &id)
{
    if (!action) return;
    // 记录绑定，以便设置变更后重新应用
    actionBindings_[id] = action;
    auto it = items_.constFind(id);
    if (it == items_.constEnd()) return;
    const auto &item = it.value();
    // 冲突项不应用快捷键
    if (item.conflicted) {
        action->setShortcut({});
        return;
    }
    const QString key = item.currentKey.isEmpty() ? item.defaultKey : item.currentKey;
    action->setShortcut(QKeySequence(key));
}

void ShortcutManager::reapplyShortcuts()
{
    for (auto it = actionBindings_.begin(); it != actionBindings_.end(); ++it) {
        if (it.value()) {
            applyToAction(it.value(), it.key());
        }
    }
}

void ShortcutManager::detectConflicts()
{
    // 收集每个快捷键对应的所有 id
    QMap<QString, QList<QString>> keyToIds;
    for (auto it = items_.begin(); it != items_.end(); ++it) {
        const QString key =
            it.value().currentKey.isEmpty() ? it.value().defaultKey : it.value().currentKey;
        if (key.isEmpty()) continue;
        keyToIds[key].append(it.key());
    }

    // 冲突项：相同快捷键的多个 id 中，第一个保持生效，其余标记为冲突
    for (auto &item : items_) {
        item.conflicted = false;
    }
    for (auto it = keyToIds.constBegin(); it != keyToIds.constEnd(); ++it) {
        const QList<QString> &ids = it.value();
        if (ids.size() <= 1) continue;
        // 第一个保持，其余标记冲突
        for (int i = 1; i < ids.size(); ++i) {
            auto itemIt = items_.find(ids.at(i));
            if (itemIt != items_.end()) {
                itemIt.value().conflicted = true;
            }
        }
    }
}

void ShortcutManager::loadFromConfig()
{
    auto *cfg = ConfigManager::instance();
    for (auto it = items_.begin(); it != items_.end(); ++it) {
        const QString key =
            cfg->value(QStringLiteral("Shortcuts"), it.key(), it.value().defaultKey).toString();
        it.value().currentKey = key;
    }
}

void ShortcutManager::saveToConfig()
{
    auto *cfg = ConfigManager::instance();
    for (auto it = items_.constBegin(); it != items_.constEnd(); ++it) {
        cfg->setValue(QStringLiteral("Shortcuts"), it.key(), it.value().currentKey);
    }
}

} // namespace fm
