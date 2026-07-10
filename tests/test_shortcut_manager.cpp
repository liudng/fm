// ShortcutManager 单元测试
// 测试快捷键初始化、查询、设置、冲突检测

#include "core/shortcut_manager.h"

#include <gtest/gtest.h>

#include <QKeySequence>

using namespace fm;

class ShortcutManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto *sm = ShortcutManager::instance();
        sm->initialize();
    }
};

// 初始化后应填充默认快捷键
TEST_F(ShortcutManagerTest, InitializePopulatesDefaults)
{
    auto *sm = ShortcutManager::instance();
    const auto shortcuts = sm->allShortcuts();
    EXPECT_FALSE(shortcuts.isEmpty());

    // 已知默认快捷键
    EXPECT_EQ(sm->shortcutFor(QStringLiteral("file.new_file")), QStringLiteral("Ctrl+N"));
    EXPECT_EQ(sm->shortcutFor(QStringLiteral("filelist.open_with")),
              QStringLiteral("Ctrl+Shift+O"));
    EXPECT_EQ(sm->shortcutFor(QStringLiteral("filelist.copy_to_opposite")), QStringLiteral("F5"));
    EXPECT_EQ(sm->shortcutFor(QStringLiteral("filelist.cut_to_opposite")), QStringLiteral("F6"));
}

// file.quit 默认无快捷键
TEST_F(ShortcutManagerTest, QuitHasNoDefaultShortcut)
{
    auto *sm = ShortcutManager::instance();
    EXPECT_TRUE(sm->shortcutFor(QStringLiteral("file.quit")).isEmpty());
}

// 设置快捷键后查询应返回新值
TEST_F(ShortcutManagerTest, SetShortcutChangesValue)
{
    auto *sm = ShortcutManager::instance();
    ASSERT_TRUE(
        sm->setShortcut(QStringLiteral("filelist.refresh"), QKeySequence(QStringLiteral("F3"))));
    EXPECT_EQ(sm->shortcutFor(QStringLiteral("filelist.refresh")), QStringLiteral("F3"));
}

// 设置冲突快捷键: 两个项使用相同快捷键
TEST_F(ShortcutManagerTest, ConflictDetection)
{
    auto *sm = ShortcutManager::instance();
    // file.new_file 默认是 Ctrl+N, 将 filelist.refresh 也设为 Ctrl+N
    sm->setShortcut(QStringLiteral("filelist.refresh"), QKeySequence(QStringLiteral("Ctrl+N")));
    sm->detectConflicts();

    const auto shortcuts = sm->allShortcuts();
    bool hasConflict = false;
    for (const auto &item : shortcuts) {
        if (item.conflicted) {
            hasConflict = true;
            break;
        }
    }
    EXPECT_TRUE(hasConflict);
}

// 不同快捷键不应产生冲突
TEST_F(ShortcutManagerTest, NoConflictForDifferentShortcuts)
{
    auto *sm = ShortcutManager::instance();
    sm->detectConflicts();

    const auto shortcuts = sm->allShortcuts();
    for (const auto &item : shortcuts) {
        EXPECT_FALSE(item.conflicted)
            << "Item " << item.id.toStdString() << " should not be conflicted";
    }
}

// 查询不存在的快捷键 ID 应返回空
TEST_F(ShortcutManagerTest, NonexistentIdReturnsEmpty)
{
    auto *sm = ShortcutManager::instance();
    EXPECT_TRUE(sm->shortcutFor(QStringLiteral("nonexistent.id")).isEmpty());
}

// 初始化后 currentKey 应等于 defaultKey
TEST_F(ShortcutManagerTest, CurrentKeyEqualsDefaultAfterInit)
{
    auto *sm = ShortcutManager::instance();
    const auto shortcuts = sm->allShortcuts();
    for (const auto &item : shortcuts) {
        EXPECT_EQ(item.currentKey, item.defaultKey)
            << "Current key for " << item.id.toStdString() << " should equal default after init";
    }
}
