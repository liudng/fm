#ifndef FM_DIALOGS_INPUT_NAME_DIALOG_H
#define FM_DIALOGS_INPUT_NAME_DIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>

class QLineEdit;
class QLabel;

namespace fm {

// 输入名称对话框（新建文件/文件夹/重命名/收藏新建）
// 支持重名校验
class InputNameDialog : public QDialog
{
    Q_OBJECT
public:
    InputNameDialog(const QString &title, const QString &label, const QString &defaultName,
                    QWidget *parent = nullptr);

    QString name() const;

    // 设置已存在的名称（用于重名校验，禁止使用）
    void setExistingNames(const QStringList &names);

private slots:
    void validate();

private:
    QLineEdit *edit_ = nullptr;
    QLabel *hintLabel_ = nullptr;
    QStringList existingNames_;
};

} // namespace fm

#endif // FM_DIALOGS_INPUT_NAME_DIALOG_H
