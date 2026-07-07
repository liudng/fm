#ifndef FM_APP_FM_APPLICATION_H
#define FM_APP_FM_APPLICATION_H

#include <QApplication>
#include <QTranslator>

namespace fm {

class MainWindow;

// 应用程序入口
// Phase 1：最小化版本，仅初始化配置与翻译；单实例在后续阶段补全
class FmApplication : public QApplication {
    Q_OBJECT
public:
    FmApplication(int &argc, char **argv);

    // 初始化配置、翻译、主窗口
    bool initialize();

    MainWindow *mainWindow() const { return mainWindow_; }

private:
    void loadTranslation(const QString &language);

    MainWindow *mainWindow_ = nullptr;
    QTranslator translator_;
};

} // namespace fm

#endif // FM_APP_FM_APPLICATION_H
