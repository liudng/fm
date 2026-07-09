#include "column_manager.h"

#include "../core/config_manager.h"
#include "../filelist/file_list_model.h"
#include "../filelist/file_list_view.h"

#include <QHeaderView>

namespace fm {

namespace {

// 列名 ↔ 枚举值 ↔ 默认像素宽度映射
struct ColumnDef {
    const char *name;
    int column;
    int defaultWidth;   // 像素；Name 列为 0（Stretch 模式）
};

const ColumnDef kColumnDefs[] = {
    {"Icon",          FileListModel::ColIcon,          28},
    {"Name",          FileListModel::ColName,          0},   // Stretch，不存储
    {"Size",          FileListModel::ColSize,          80},
    {"Type",          FileListModel::ColType,          100},
    {"MimeType",      FileListModel::ColMimeType,      120},
    {"Group",         FileListModel::ColGroup,         80},
    {"Owner",         FileListModel::ColOwner,         80},
    {"UID",           FileListModel::ColOwnerUid,      60},
    {"GID",           FileListModel::ColGroupGid,      60},
    {"Created",       FileListModel::ColCreated,       140},
    {"Modified",      FileListModel::ColModified,      140},
    {"Accessed",      FileListModel::ColAccessed,     140},
    {"Disk Usage",    FileListModel::ColDiskUsage,     100},
    {"Status Changed",FileListModel::ColStatusChanged,140},
    {"Permissions",   FileListModel::ColPermissions,   100},
};
const int kColumnCount = sizeof(kColumnDefs) / sizeof(ColumnDef);

// 最小列宽（像素）
constexpr int kMinColumnWidth = 20;

QString columnName(int col) {
    for (int i = 0; i < kColumnCount; ++i) {
        if (kColumnDefs[i].column == col) return QString::fromLatin1(kColumnDefs[i].name);
    }
    return {};
}

int columnEnum(const QString &name) {
    for (int i = 0; i < kColumnCount; ++i) {
        if (QLatin1String(kColumnDefs[i].name) == name) return kColumnDefs[i].column;
    }
    return -1;
}

} // namespace

ColumnManager *ColumnManager::instance() {
    static ColumnManager inst;
    return &inst;
}

ColumnManager::ColumnManager(QObject *parent)
    : QObject(parent) {
    // 默认全部不可见，由 loadFromConfig 设置
    for (int i = 0; i < kColumnCount; ++i) {
        const QString name = QString::fromLatin1(kColumnDefs[i].name);
        visibleMap_[name] = false;
        widthMap_[name] = kColumnDefs[i].defaultWidth;
    }
}

QStringList ColumnManager::allColumnNames() const {
    QStringList names;
    for (int i = 0; i < kColumnCount; ++i) {
        names << QString::fromLatin1(kColumnDefs[i].name);
    }
    return names;
}

bool ColumnManager::isColumnVisible(const QString &columnName) const {
    return visibleMap_.value(columnName, false);
}

void ColumnManager::setColumnVisible(const QString &columnName, bool visible) {
    if (visibleMap_.value(columnName) == visible) return;
    visibleMap_[columnName] = visible;
    // 若顺序中不包含且变为可见，则追加
    if (visible && !order_.contains(columnName)) {
        order_.append(columnName);
    } else if (!visible) {
        order_.removeAll(columnName);
    }
    saveToConfig();
    applyToAllViews();
    emit columnsChanged();
}

int ColumnManager::columnWidth(const QString &columnName) const {
    return widthMap_.value(columnName, 80);
}

void ColumnManager::setColumnWidth(const QString &columnName, int width) {
    if (columnName == QStringLiteral("Name")) return;  // Name 列 Stretch，不存储
    widthMap_[columnName] = qMax(kMinColumnWidth, width);
    saveToConfig();
    applyToAllViews();
    emit columnsChanged();
}

QStringList ColumnManager::columnOrder() const {
    return order_;
}

void ColumnManager::setColumnOrder(const QStringList &order) {
    order_ = order;
    saveToConfig();
    applyToAllViews();
    emit columnsChanged();
}

void ColumnManager::loadFromConfig() {
    auto *cfg = ConfigManager::instance();

    // 读取可见列与顺序（逗号分隔字符串）
    const QString colsStr =
        cfg->value(QStringLiteral("File_Browser_Columns"), QStringLiteral("columns"),
                   QStringLiteral("Icon,Name,Size,Modified")).toString();
    order_ = colsStr.split(QLatin1Char(','), Qt::SkipEmptyParts);

    // 重置可见性
    for (auto it = visibleMap_.begin(); it != visibleMap_.end(); ++it) {
        it.value() = false;
    }
    // 标记 order_ 中的列为可见
    for (const QString &n : order_) {
        visibleMap_[n] = true;
    }

    // 读取像素宽度（Name 列跳过）
    for (int i = 0; i < kColumnCount; ++i) {
        const QString name = QString::fromLatin1(kColumnDefs[i].name);
        if (kColumnDefs[i].defaultWidth == 0) continue;  // Name 列 Stretch
        const int width = cfg->value(QStringLiteral("File_Browser_Columns"),
                                      QStringLiteral("width_") + name,
                                      kColumnDefs[i].defaultWidth).toInt();
        widthMap_[name] = qMax(kMinColumnWidth, width);
    }
    applyToAllViews();
}

void ColumnManager::saveToConfig() {
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("File_Browser_Columns"), QStringLiteral("columns"),
                   order_.join(QLatin1Char(',')));
    for (int i = 0; i < kColumnCount; ++i) {
        const QString name = QString::fromLatin1(kColumnDefs[i].name);
        if (kColumnDefs[i].defaultWidth == 0) continue;  // Name 列不存储
        cfg->setValue(QStringLiteral("File_Browser_Columns"),
                       QStringLiteral("width_") + name, widthMap_.value(name));
    }
}

void ColumnManager::applyToView(FileListView *view) {
    if (!view) return;

    // 收集可见列与对应像素宽度
    QList<int> visibleCols;
    QMap<int, int> widths;
    for (const QString &name : order_) {
        if (!visibleMap_.value(name, false)) continue;
        const int col = columnEnum(name);
        if (col < 0) continue;
        visibleCols.append(col);
        if (col != FileListModel::ColName) {
            widths[col] = qMax(kMinColumnWidth, widthMap_.value(name, 80));
        }
    }
    if (visibleCols.isEmpty()) {
        // 至少保证 Name 列可见
        visibleCols.append(FileListModel::ColName);
    }
    // 防止 setColumnConfig 触发的 sectionResized/sectionMoved 信号导致递归
    applying_ = true;
    view->setColumnConfig(visibleCols, widths);
    applying_ = false;
}

void ColumnManager::registerView(FileListView *view) {
    if (!view || views_.contains(view)) return;
    views_.append(view);
    applyToView(view);

    // 监听 header 的用户拖拽调整列宽与重排列
    auto *header = view->header();
    connect(header, &QHeaderView::sectionResized, this,
            [this, view](int logical, int oldSize, int newSize) {
                onSectionResized(view, logical, oldSize, newSize);
            });
    connect(header, &QHeaderView::sectionMoved, this,
            [this, view](int logical, int oldVisual, int newVisual) {
                onSectionMoved(view, logical, oldVisual, newVisual);
            });
}

void ColumnManager::unregisterView(FileListView *view) {
    if (!view) return;
    views_.removeAll(view);
    // 断开该 view header 到 this 的所有连接
    if (view->header()) {
        view->header()->disconnect(this);
    }
}

void ColumnManager::applyToAllViews() {
    for (FileListView *view : views_) {
        applyToView(view);
    }
}

void ColumnManager::onSectionResized(FileListView *view, int logicalIndex,
                                       int oldSize, int newSize) {
    Q_UNUSED(oldSize);
    if (applying_ || !view) return;
    // Name 列是 Stretch 模式，宽度自动变化，不存储
    if (logicalIndex == FileListModel::ColName) return;
    const QString name = columnName(logicalIndex);
    if (name.isEmpty()) return;
    widthMap_[name] = qMax(kMinColumnWidth, newSize);
    saveToConfig();
}

void ColumnManager::onSectionMoved(FileListView *view, int logical,
                                     int oldVisual, int newVisual) {
    if (applying_ || !view) return;
    Q_UNUSED(oldVisual);
    Q_UNUSED(newVisual);
    // 根据 header 当前视觉顺序重建 order_
    auto *header = view->header();
    QStringList newOrder;
    for (int visual = 0; visual < header->count(); ++visual) {
        const int logicalIdx = header->logicalIndex(visual);
        if (!header->isSectionHidden(logicalIdx)) {
            const QString name = columnName(logicalIdx);
            if (!name.isEmpty() && visibleMap_.value(name, false)) {
                newOrder.append(name);
            }
        }
    }
    order_ = newOrder;
    saveToConfig();
}

} // namespace fm
