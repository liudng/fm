#ifndef FM_DIALOGS_SETTINGS_DIALOG_H
#define FM_DIALOGS_SETTINGS_DIALOG_H

#include <QDialog>
#include <QList>
#include <QString>

class QListWidget;
class QStackedWidget;
class QPushButton;

namespace fm {

class ISettingsPage;

// 设置对话框
// - 左侧 1/5 宽度 sidebar 显示分组（界面、面板、文件浏览、快捷键）
// - 右侧显示对应分组内容
// - 底部按钮：取消/应用/确定
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // 显示指定页（用于"设置列"等调用方）
    void showPage(const QString &pageId);

    // 添加自定义页（用于扩展）
    void addPage(ISettingsPage *page);

private slots:
    void onPageChanged(int index);
    void onApply();
    void onOk();
    void onCancel();

private:
    QList<ISettingsPage*> pages_;
    QListWidget *sidebar_ = nullptr;
    QStackedWidget *contentStack_ = nullptr;
    QPushButton *applyBtn_ = nullptr;
};

} // namespace fm

#endif // FM_DIALOGS_SETTINGS_DIALOG_H
