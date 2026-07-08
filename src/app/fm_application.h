#ifndef FM_APP_FM_APPLICATION_H
#define FM_APP_FM_APPLICATION_H

#include <QApplication>
#include <QTranslator>

namespace fm {

class MainWindow;
class SingleInstance;

// 应用程序入口
// - 单实例检测
// - 配置加载、翻译、主窗口初始化
class FmApplication : public QApplication {
    Q_OBJECT
public:
    FmApplication(int &argc, char **argv);

    // 初始化配置、翻译、主窗口
    // 返回 false 表示应退出（单实例已有或配置损坏且用户选择退出）
    bool initialize();

    MainWindow *mainWindow() const { return mainWindow_; }

private:
    void loadTranslation(const QString &language);
    // 从配置读取并应用图标主题（UI/iconTheme）
    void applyIconTheme();

    MainWindow *mainWindow_ = nullptr;
    QTranslator translator_;
    SingleInstance *singleInstance_ = nullptr;
};

} // namespace fm

#endif // FM_APP_FM_APPLICATION_H
