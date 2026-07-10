// ConfigManager 单元测试
// 测试配置读写、默认值、损坏检测、重建

#include "core/config_manager.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace fm;

class ConfigManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto *cfg = ConfigManager::instance();
        // rebuild() 清除旧文件并写入默认值，确保测试隔离
        cfg->rebuild();
    }
};

// 默认配置应正确写入
TEST_F(ConfigManagerTest, DefaultConfigHasExpectedValues)
{
    auto *cfg = ConfigManager::instance();
    EXPECT_EQ(cfg->value(QStringLiteral("UI"), QStringLiteral("language")).toString(),
              QStringLiteral("en"));
    EXPECT_EQ(cfg->value(QStringLiteral("UI"), QStringLiteral("theme")).toString(),
              QStringLiteral("Fusion"));
    EXPECT_EQ(cfg->value(QStringLiteral("File_Operations"), QStringLiteral("chunkSizeMB")).toInt(),
              1);
    EXPECT_FALSE(cfg->value(QStringLiteral("File_Browser"), QStringLiteral("showHidden")).toBool());
}

// setValue + value 往返
TEST_F(ConfigManagerTest, SetValueRoundTrip)
{
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("Test"), QStringLiteral("key1"), QStringLiteral("hello"));
    EXPECT_EQ(cfg->value(QStringLiteral("Test"), QStringLiteral("key1")).toString(),
              QStringLiteral("hello"));
}

// contains 检测
TEST_F(ConfigManagerTest, ContainsReturnsCorrectStatus)
{
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("Test"), QStringLiteral("exists"), 42);
    EXPECT_TRUE(cfg->contains(QStringLiteral("Test"), QStringLiteral("exists")));
    EXPECT_FALSE(cfg->contains(QStringLiteral("Test"), QStringLiteral("nonexistent")));
}

// 不存在的键返回默认值
TEST_F(ConfigManagerTest, NonexistentKeyReturnsDefault)
{
    auto *cfg = ConfigManager::instance();
    EXPECT_EQ(cfg->value(QStringLiteral("Test"), QStringLiteral("nonexistent"),
                         QStringLiteral("fallback"))
                  .toString(),
              QStringLiteral("fallback"));
}

// 整数类型读写
TEST_F(ConfigManagerTest, IntegerRoundTrip)
{
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("Test"), QStringLiteral("int1"), 12345);
    EXPECT_EQ(cfg->value(QStringLiteral("Test"), QStringLiteral("int1")).toInt(), 12345);
}

// 布尔类型读写
TEST_F(ConfigManagerTest, BoolRoundTrip)
{
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("Test"), QStringLiteral("bool1"), true);
    cfg->setValue(QStringLiteral("Test"), QStringLiteral("bool2"), false);
    EXPECT_TRUE(cfg->value(QStringLiteral("Test"), QStringLiteral("bool1")).toBool());
    EXPECT_FALSE(cfg->value(QStringLiteral("Test"), QStringLiteral("bool2")).toBool());
}

// 列表类型读写
TEST_F(ConfigManagerTest, ListRoundTrip)
{
    auto *cfg = ConfigManager::instance();
    QStringList list{"a", "b", "c"};
    cfg->setValue(QStringLiteral("Test"), QStringLiteral("list1"), list);
    EXPECT_EQ(cfg->value(QStringLiteral("Test"), QStringLiteral("list1")).toStringList(), list);
}

// rebuild 后应恢复默认配置
TEST_F(ConfigManagerTest, RebuildRestoresDefaults)
{
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("Test"), QStringLiteral("temp"), QStringLiteral("dirty"));
    ASSERT_TRUE(cfg->contains(QStringLiteral("Test"), QStringLiteral("temp")));

    cfg->rebuild();

    EXPECT_FALSE(cfg->contains(QStringLiteral("Test"), QStringLiteral("temp")));
    // 默认值应恢复
    EXPECT_EQ(cfg->value(QStringLiteral("UI"), QStringLiteral("language")).toString(),
              QStringLiteral("en"));
}

// 配置文件路径应存在
TEST_F(ConfigManagerTest, FilePathPointsToExistingFile)
{
    auto *cfg = ConfigManager::instance();
    EXPECT_TRUE(QFileInfo::exists(cfg->filePath()));
}
