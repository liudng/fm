#ifndef FM_FILEOPS_PROGRESS_DIALOG_H
#define FM_FILEOPS_PROGRESS_DIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QProgressBar;
class QPushButton;

namespace fm {

// 文件操作进度对话框
// - 显示进度条、当前文件名、取消按钮
// - 通过阈值控制显示（>1s 才显示）
class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(QWidget *parent = nullptr);

    // 设置标题
    void setOperationTitle(const QString &title);

    // 设置总进度（0-100，-1 表示不确定）
    void setProgress(int percent);

    // 设置当前处理的文件名
    void setCurrentFile(const QString &fileName);

    // 延迟显示：调用后启动定时器，1s 后若未被取消则 show()
    void showDelayed();

    bool wasCanceled() const { return canceled_; }

signals:
    void canceled();

private:
    QLabel *titleLabel_ = nullptr;
    QLabel *fileLabel_ = nullptr;
    QProgressBar *progressBar_ = nullptr;
    QPushButton *cancelBtn_ = nullptr;
    bool canceled_ = false;
    int showTimerId_ = 0;

    void timerEvent(QTimerEvent *event) override;
};

} // namespace fm

#endif // FM_FILEOPS_PROGRESS_DIALOG_H
