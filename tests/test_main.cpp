// GTest 自定义 main: 创建 QCoreApplication 供 Qt 类使用
// QStandardPaths::setTestModeEnabled(true) 将配置/缓存重定向到临时目录, 避免污染用户环境

#include <QCoreApplication>
#include <QStandardPaths>

#include <gtest/gtest.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
