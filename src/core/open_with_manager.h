#ifndef FM_CORE_OPEN_WITH_MANAGER_H
#define FM_CORE_OPEN_WITH_MANAGER_H

#include <QString>

namespace fm {

// "打开方式"管理
// "记住此选择"直接修改用户级系统 MIME 关联（~/.config/mimeapps.list 的
// [Default Applications] 节），不保存到 fm 私有配置文件：
// - 标准目录中的 .desktop → 直接引用其文件名写入关联
// - 非标准路径的 .desktop → 复制到 ~/.local/share/applications/ 再写入关联
// - 自定义命令 → 生成 fm-custom-<hash>.desktop 安装到 ~/.local/share/applications/
class OpenWithManager
{
public:
    // 设置 MIME 类型的默认应用（写入用户级系统关联）
    // app：.desktop 文件路径或自定义命令行
    // 返回是否成功
    static bool setDefaultApplication(const QString &mimeType, const QString &app);

private:
    // 将 app 注册为标准 applications 目录中的 .desktop 文件，返回文件名
    static QString registerApplication(const QString &app);

    // 更新 mimeapps.list [Default Applications]：<mimeType>=<desktopName>
    static bool updateMimeappsDefault(const QString &mimeType, const QString &desktopName);

    OpenWithManager() = default;
};

} // namespace fm

#endif // FM_CORE_OPEN_WITH_MANAGER_H
