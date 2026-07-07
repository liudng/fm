#ifndef FM_DIALOGS_CONFLICT_DIALOG_H
#define FM_DIALOGS_CONFLICT_DIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;

namespace fm {

// 同名冲突处理方式
enum class ConflictResolution {
    Overwrite,
    Skip,
    Rename,
    OverwriteAll,
    SkipAll,
    RenameAll,
    Cancel
};

// 冲突选择对话框
// 显示源文件名与目标路径，提供 6 种处理方式 + 取消
class ConflictDialog : public QDialog {
    Q_OBJECT
public:
    ConflictDialog(const QString &sourceName, const QString &destPath,
                   bool allowBatch, QWidget *parent = nullptr);

    ConflictResolution resolution() const { return resolution_; }

private slots:
    void chooseOverwrite();
    void chooseSkip();
    void chooseRename();
    void chooseOverwriteAll();
    void chooseSkipAll();
    void chooseRenameAll();

private:
    ConflictResolution resolution_ = ConflictResolution::Cancel;
};

} // namespace fm

#endif // FM_DIALOGS_CONFLICT_DIALOG_H
