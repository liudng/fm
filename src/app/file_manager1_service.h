#ifndef FM_APP_FILE_MANAGER1_SERVICE_H
#define FM_APP_FILE_MANAGER1_SERVICE_H

#include <QObject>
#include <QStringList>

namespace fm {

// org.freedesktop.FileManager1 D-Bus 服务（freedesktop.org 文件管理器接口规范）
// - 供浏览器"打开所在文件夹"、桌面"显示属性"等场景跨进程调用
// - 服务名：org.freedesktop.FileManager1，对象路径：/org/freedesktop/FileManager1
// - 协议方法：ShowFolders(as,s) / ShowItems(as,s) / ShowItemProperties(as,s)
//
// 本类通过 QDBusConnection::ExportAllSlots 导出方法：
// 全部 slot 均成为 D-Bus 方法，禁止添加其他 slot（内部逻辑用普通函数或信号）。
class FileManager1Service : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")
public:
    explicit FileManager1Service(QObject *parent = nullptr);

    // 在 session bus 上注册服务名与对象
    // 注册失败（无会话总线，或服务名已被其他文件管理器占用）时仅记录日志并
    // 返回 false，程序其余功能不受影响。
    bool registerService();

public slots:
    // ---- org.freedesktop.FileManager1 协议方法 ----

    // 显示文件夹：每个文件夹在活动面板打开（已有同路径选项卡则切换）
    void ShowFolders(const QStringList &uris, const QString &startupId);

    // 显示项目：打开项目所在文件夹并选中项目（按父目录分组，每组一个选项卡）
    void ShowItems(const QStringList &uris, const QString &startupId);

    // 显示项目属性：弹出属性对话框
    void ShowItemProperties(const QStringList &uris, const QString &startupId);

signals:
    // 协议方法换算为本地路径后发射（仅保留本地 file:// URI）
    void showFoldersRequested(const QStringList &paths);
    void showItemsRequested(const QStringList &paths);
    void showItemPropertiesRequested(const QStringList &paths);

private:
    // URI 列表 → 本地路径列表（忽略非本地 URI；容忍无 scheme 的绝对路径）
    static QStringList urisToLocalPaths(const QStringList &uris);
};

} // namespace fm

#endif // FM_APP_FILE_MANAGER1_SERVICE_H
