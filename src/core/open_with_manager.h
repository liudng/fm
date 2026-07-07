#ifndef FM_CORE_OPEN_WITH_MANAGER_H
#define FM_CORE_OPEN_WITH_MANAGER_H

#include <QObject>
#include <QString>

namespace fm {

// "打开方式"管理（单例）
// - 持久化 [OpenWith] section 的 MIME 类型 → 应用映射
// - 格式：<MIME类型>=<应用.desktop 文件路径>
// - 自动用 xdg-mime 查询默认应用
class OpenWithManager : public QObject {
    Q_OBJECT
public:
    static OpenWithManager *instance();

    // 查询 MIME 类型的"记住"应用 .desktop 路径
    // 返回空表示未配置
    QString defaultApplication(const QString &mimeType) const;

    // 设置 MIME 类型的默认应用
    void setDefaultApplication(const QString &mimeType, const QString &desktopFile);

    // 通过 xdg-mime 查询系统默认应用
    static QString systemDefault(const QString &mimeType);

private:
    OpenWithManager(QObject *parent = nullptr);
};

} // namespace fm

#endif // FM_CORE_OPEN_WITH_MANAGER_H
