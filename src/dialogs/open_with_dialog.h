#ifndef FM_DIALOGS_OPEN_WITH_DIALOG_H
#define FM_DIALOGS_OPEN_WITH_DIALOG_H

#include <QDialog>
#include <QString>

class QListWidget;
class QCheckBox;
class QLineEdit;

namespace fm {

// "打开方式"对话框
// - 通过 xdg-mime 列举可用程序
// - 显示应用名称+图标
// - 支持"其他..."自定义命令
// - 支持"记住此选择"复选框
class OpenWithDialog : public QDialog {
    Q_OBJECT
public:
    OpenWithDialog(const QString &mimeType, const QString &fileName, QWidget *parent = nullptr);

    // 返回选中的 .desktop 文件路径（或自定义命令）
    QString selectedApplication() const;

    // 是否勾选"记住此选择"
    bool rememberChoice() const;

    // 是否选择了自定义命令
    bool isCustomCommand() const;

private slots:
    void onSelectionChanged();
    void onCustomBrowse();

private:
    void populateApplications(const QString &mimeType);

    QString mimeType_;
    QListWidget *appList_ = nullptr;
    QCheckBox *rememberCheck_ = nullptr;
    QLineEdit *customEdit_ = nullptr;
    QString selectedDesktopFile_;
};

} // namespace fm

#endif // FM_DIALOGS_OPEN_WITH_DIALOG_H
