#ifndef FM_DIALOGS_ERROR_DIALOG_H
#define FM_DIALOGS_ERROR_DIALOG_H

#include <QString>

class QWidget;

namespace fm {

// 错误提示对话框（静态工具类）
class ErrorDialog {
public:
    static void show(QWidget *parent, const QString &title, const QString &message);
    static void show(QWidget *parent, const QString &message);  // 标题默认"错误"
};

} // namespace fm

#endif // FM_DIALOGS_ERROR_DIALOG_H
