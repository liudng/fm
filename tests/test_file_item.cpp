// makeFileItem 单元测试
// 测试从 QFileInfo 构造 FileItem 的字段填充（名称、类型、stat 字段、MIME）

#include "filelist/file_item.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

using namespace fm;

namespace {

// 递归返回目录自身信息也由同一函数处理，这里只测常规文件/目录两种
QTemporaryDir &tempRoot()
{
    static QTemporaryDir dir;
    return dir;
}

} // namespace

// 常规文件：基础字段与 stat 字段应填充
TEST(MakeFileItemTest, RegularFileFields)
{
    const QString path = tempRoot().filePath(QStringLiteral("hello.txt"));
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("0123456789");
    f.close();

    const QFileInfo fi(path);
    ASSERT_TRUE(fi.exists());

    const FileItem item = makeFileItem(fi);
    EXPECT_EQ(item.name, QStringLiteral("hello.txt"));
    EXPECT_EQ(item.absolutePath, QFileInfo(path).absoluteFilePath());
    EXPECT_FALSE(item.isDir);
    EXPECT_FALSE(item.isSymLink);
    EXPECT_EQ(item.size, 10);
    EXPECT_GT(item.diskUsage, 0); // st_blocks * 512 至少为一块
    EXPECT_GT(item.inode, 0);
    EXPECT_EQ(item.ownerId, fi.ownerId());
    EXPECT_EQ(item.groupId, fi.groupId());
    EXPECT_EQ(item.owner, fi.owner());
    EXPECT_EQ(item.group, fi.group());
    EXPECT_TRUE(item.modified.isValid());
    EXPECT_TRUE(item.statusChanged.isValid());
    EXPECT_EQ(item.mimeTypeName, QStringLiteral("text/plain"));
    EXPECT_FALSE(item.mimeTypeComment.isEmpty());
}

// 目录：isDir 为真，size 为 0
TEST(MakeFileItemTest, DirectoryFields)
{
    const QString path = tempRoot().filePath(QStringLiteral("subdir"));
    ASSERT_TRUE(QDir().mkpath(path));

    const FileItem item = makeFileItem(QFileInfo(path));
    EXPECT_EQ(item.name, QStringLiteral("subdir"));
    EXPECT_TRUE(item.isDir);
    EXPECT_EQ(item.size, 0);
    EXPECT_GT(item.inode, 0);
}

// 符号链接：isSymLink 为真且填充直接目标（不做多层解析）
TEST(MakeFileItemTest, SymlinkFields)
{
    const QString target = tempRoot().filePath(QStringLiteral("target.bin"));
    QFile f(target);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    const QString link = tempRoot().filePath(QStringLiteral("link.bin"));
    if (QFile::exists(link)) QFile::remove(link);
    ASSERT_TRUE(QFile::link(target, link));

    const FileItem item = makeFileItem(QFileInfo(link));
    EXPECT_TRUE(item.isSymLink);
    EXPECT_EQ(item.symLinkTarget, target);
}
