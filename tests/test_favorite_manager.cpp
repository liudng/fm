// FavoriteManager 单元测试
// 测试收藏的增删查、名称编码解码、布局状态序列化

#include "core/favorite_manager.h"
#include "core/session_state.h"

#include <gtest/gtest.h>

#include <QUrl>

using namespace fm;

class FavoriteManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto *fm = FavoriteManager::instance();
        // 清理已有收藏, 确保测试隔离
        const auto names = fm->favoriteNames();
        for (const auto &name : names) {
            fm->removeFavorite(name);
        }
    }
};

// 添加收藏
TEST_F(FavoriteManagerTest, AddFavorite)
{
    auto *fm = FavoriteManager::instance();
    LayoutState state;
    state.panels[0].tabs.append({"path1", 1, Qt::AscendingOrder});
    state.panels[1].tabs.append({"path2", 1, Qt::AscendingOrder});

    EXPECT_TRUE(fm->addFavorite(QStringLiteral("Test1"), state));
    const auto names = fm->favoriteNames();
    EXPECT_EQ(names.size(), 1);
    EXPECT_TRUE(names.contains(QStringLiteral("Test1")));
}

// 重名添加应失败
TEST_F(FavoriteManagerTest, AddDuplicateFails)
{
    auto *fm = FavoriteManager::instance();
    LayoutState state;
    EXPECT_TRUE(fm->addFavorite(QStringLiteral("Dup"), state));
    EXPECT_FALSE(fm->addFavorite(QStringLiteral("Dup"), state));
    EXPECT_EQ(fm->favoriteNames().size(), 1);
}

// 删除收藏
TEST_F(FavoriteManagerTest, RemoveFavorite)
{
    auto *fm = FavoriteManager::instance();
    LayoutState state;
    fm->addFavorite(QStringLiteral("ToRemove"), state);
    ASSERT_EQ(fm->favoriteNames().size(), 1);

    EXPECT_TRUE(fm->removeFavorite(QStringLiteral("ToRemove")));
    EXPECT_EQ(fm->favoriteNames().size(), 0);
}

// 删除不存在的收藏应失败
TEST_F(FavoriteManagerTest, RemoveNonexistentFails)
{
    auto *fm = FavoriteManager::instance();
    EXPECT_FALSE(fm->removeFavorite(QStringLiteral("Nonexistent")));
}

// 加载收藏布局
TEST_F(FavoriteManagerTest, LoadFavoriteLayout)
{
    auto *fm = FavoriteManager::instance();
    LayoutState original;
    original.panels[0].tabs.append({"/tmp", 1, Qt::AscendingOrder});
    original.panels[1].tabs.append({"/home", 2, Qt::DescendingOrder});

    ASSERT_TRUE(fm->addFavorite(QStringLiteral("Layout1"), original));

    LayoutState loaded;
    ASSERT_TRUE(fm->loadFavorite(QStringLiteral("Layout1"), loaded));
    EXPECT_EQ(loaded.panels[0].tabs.size(), 1);
    EXPECT_EQ(loaded.panels[0].tabs[0].path, QStringLiteral("/tmp"));
    EXPECT_EQ(loaded.panels[1].tabs.size(), 1);
    EXPECT_EQ(loaded.panels[1].tabs[0].path, QStringLiteral("/home"));
}

// 名称含特殊字符（percent-encoding 透明处理）
TEST_F(FavoriteManagerTest, SpecialCharacterNames)
{
    auto *fm = FavoriteManager::instance();
    LayoutState state;

    // 名称含斜杠等特殊字符
    EXPECT_TRUE(fm->addFavorite(QStringLiteral("a/b=c"), state));
    EXPECT_TRUE(fm->favoriteNames().contains(QStringLiteral("a/b=c")));

    LayoutState loaded;
    EXPECT_TRUE(fm->loadFavorite(QStringLiteral("a/b=c"), loaded));
    EXPECT_TRUE(fm->removeFavorite(QStringLiteral("a/b=c")));
}

// 空名称
TEST_F(FavoriteManagerTest, EmptyName)
{
    auto *fm = FavoriteManager::instance();
    LayoutState state;
    // 空名称的行为取决于实现, 但不应崩溃
    fm->addFavorite(QStringLiteral(""), state);
}
