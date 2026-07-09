#ifndef FM_FILELIST_FILE_LIST_MODEL_H
#define FM_FILELIST_FILE_LIST_MODEL_H

#include "../filelist/file_item.h"

#include <QAbstractItemModel>
#include <QList>
#include <QString>

namespace fm {

// 文件列表模型
// - 第 0 行固定为 ".."（根目录下隐藏）
// - 行数据来自目录枚举
class FileListModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column {
        ColIcon = 0,
        ColName,
        ColSize,
        ColType,
        ColMimeType,
        ColGroup,
        ColOwner,
        ColCreated,
        ColModified,
        ColPermissions,
        ColCount
    };
    Q_ENUM(Column)

    enum Role {
        FileItemRole = Qt::UserRole + 1,   // 返回完整 FileItem
        IsParentRowRole,                    // 是否 ".." 行
    };

    explicit FileListModel(QObject *parent = nullptr);

    // 路径与加载
    QString path() const { return path_; }
    void setPath(const QString &path);
    void reload();

    bool showHidden() const { return showHidden_; }
    void setShowHidden(bool show);

    // 日期时间显示格式（空字符串表示使用默认 ISO 格式）
    QString dateTimeFormat() const { return dateTimeFormat_; }
    void setDateTimeFormat(const QString &format);

    // 行数据访问
    FileItem itemAt(const QModelIndex &index) const;
    bool isParentRow(const QModelIndex &index) const;

    // QAbstractItemModel 接口
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

signals:
    void pathChanged(const QString &path);
    void loadError(const QString &errorMsg);

private:
    // 行号 0 永远为 ".."（若 hasParent_ 为真）
    bool hasParentRow() const;
    int parentRowOffset() const;     // 0 或 1
    void loadDirectory();

    QString path_;
    QList<FileItem> items_;
    bool showHidden_ = false;
    bool hasParent_ = false;          // 是否存在 ".." 行
    QString lastError_;
    QString dateTimeFormat_;          // 日期时间显示格式（空=ISO 默认）
};

} // namespace fm

#endif // FM_FILELIST_FILE_LIST_MODEL_H
