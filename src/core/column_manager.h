#ifndef FM_CORE_COLUMN_MANAGER_H
#define FM_CORE_COLUMN_MANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

namespace fm {

class FileListView;

// 列管理（单例）
// - 维护所有列的可见性、宽度比例、顺序
// - 持久化到 [File_Browser_Columns]
// - 应用到所有 FileListView
class ColumnManager : public QObject {
    Q_OBJECT
public:
    static ColumnManager *instance();

    // 列名（与 FileListModel::Column 对应，字符串形式用于持久化）
    // Icon, Name, Size, Type, MimeType, Group, Owner, Created, Modified, Permissions

    // 列的可见性（按字符串名）
    bool isColumnVisible(const QString &columnName) const;
    void setColumnVisible(const QString &columnName, bool visible);

    // 列宽比例（0.0~1.0）
    double widthRatio(const QString &columnName) const;
    void setWidthRatio(const QString &columnName, double ratio);

    // 列顺序（字符串列表）
    QStringList columnOrder() const;
    void setColumnOrder(const QStringList &order);

    // 加载并应用配置到所有视图
    void loadFromConfig();
    void saveToConfig();

    // 应用到单个视图
    void applyToView(FileListView *view);

    // 应用到所有已注册视图
    void registerView(FileListView *view);
    void unregisterView(FileListView *view);
    void applyToAllViews();

    // 获取所有列名（用于设置页显示）
    QStringList allColumnNames() const;

signals:
    void columnsChanged();

private:
    ColumnManager(QObject *parent = nullptr);

    // 用户交互时由 header 信号触发，更新内存中的比例/顺序并保存
    void onSectionResized(FileListView *view, int logicalIndex, int oldSize, int newSize);
    void onSectionMoved(FileListView *view, int logical, int oldVisualIndex, int newVisualIndex);

    QList<FileListView *> views_;
    QMap<QString, bool> visibleMap_;       // columnName -> visible
    QMap<QString, double> ratioMap_;       // columnName -> ratio
    QStringList order_;                     // 列顺序
    bool applying_ = false;                // 防止 applyToView 触发的信号递归
};

} // namespace fm

#endif // FM_CORE_COLUMN_MANAGER_H
