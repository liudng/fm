#ifndef FM_UI_MAIN_WINDOW_H
#define FM_UI_MAIN_WINDOW_H

#include <QMainWindow>

namespace fm {

class PanelContainer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    PanelContainer *panelContainer() const { return panelContainer_; }

    // 单实例接收到 path 列表时调用
    void addPathsToPanels(const QStringList &paths);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onExit();

private:
    void buildMenuBar();
    void restoreSession();

    PanelContainer *panelContainer_ = nullptr;
};

} // namespace fm

#endif // FM_UI_MAIN_WINDOW_H
