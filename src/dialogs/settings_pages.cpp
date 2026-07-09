#include "settings_pages.h"

#include "../core/column_manager.h"
#include "../core/config_manager.h"
#include "../core/shortcut_manager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QStyleFactory>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace fm {

// ============ UiSettingsPage ============

UiSettingsPage::UiSettingsPage(QObject *parent)
    : QObject(parent) {
    widget_ = new QWidget;

    auto *layout = new QVBoxLayout(widget_);

    // 语言
    auto *langBox = new QGroupBox(tr("Language"));
    auto *langLayout = new QFormLayout(langBox);
    langCombo_ = new QComboBox;
    langCombo_->addItem(tr("English"), QStringLiteral("en"));
    langCombo_->addItem(tr("Chinese"), QStringLiteral("zh"));
    langLayout->addRow(tr("Language:"), langCombo_);
    layout->addWidget(langBox);

    // 主题
    auto *themeBox = new QGroupBox(tr("Theme"));
    auto *themeLayout = new QFormLayout(themeBox);
    themeCombo_ = new QComboBox;
    themeCombo_->addItem(tr("Default"), QString());
    for (const QString &key : QStyleFactory::keys()) {
        themeCombo_->addItem(key, key);
    }
    themeLayout->addRow(tr("Theme:"), themeCombo_);
    layout->addWidget(themeBox);

    // 图标主题
    auto *iconBox = new QGroupBox(tr("Icon Theme"));
    auto *iconLayout = new QFormLayout(iconBox);
    iconCombo_ = new QComboBox;
    // 第一项：自动（空值表示由程序根据可用性选择，默认 gnome）
    iconCombo_->addItem(tr("Automatic"), QString());
    // 枚举系统中已安装的图标主题（含 index.theme 的目录）
    QSet<QString> themes;
    for (const QString &path : QIcon::themeSearchPaths()) {
        QDir dir(path);
        if (!dir.exists()) continue;
        const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &name : entries) {
            if (QFileInfo(dir.absoluteFilePath(name + QStringLiteral("/index.theme"))).exists()) {
                themes.insert(name);
            }
        }
    }
    QStringList sorted = themes.values();
    sorted.sort();
    for (const QString &name : sorted) {
        iconCombo_->addItem(name, name);
    }
    iconLayout->addRow(tr("Icon theme:"), iconCombo_);
    layout->addWidget(iconBox);

    layout->addStretch(1);
}

QString UiSettingsPage::title() const { return tr("Interface"); }

void UiSettingsPage::load() {
    auto *cfg = ConfigManager::instance();
    origLang_ = cfg->value(QStringLiteral("UI"), QStringLiteral("language"),
                            QStringLiteral("en")).toString();
    origTheme_ = cfg->value(QStringLiteral("UI"), QStringLiteral("theme"),
                             QStringLiteral("Fusion")).toString();
    origIconTheme_ = cfg->value(QStringLiteral("UI"), QStringLiteral("iconTheme"),
                                  QString()).toString();

    // 选择当前值
    for (int i = 0; i < langCombo_->count(); ++i) {
        if (langCombo_->itemData(i).toString() == origLang_) {
            langCombo_->setCurrentIndex(i);
            break;
        }
    }
    for (int i = 0; i < themeCombo_->count(); ++i) {
        if (themeCombo_->itemData(i).toString() == origTheme_) {
            themeCombo_->setCurrentIndex(i);
            break;
        }
    }
    for (int i = 0; i < iconCombo_->count(); ++i) {
        if (iconCombo_->itemData(i).toString() == origIconTheme_) {
            iconCombo_->setCurrentIndex(i);
            break;
        }
    }
}

void UiSettingsPage::apply() {
    auto *cfg = ConfigManager::instance();
    const QString lang = langCombo_->currentData().toString();
    const QString theme = themeCombo_->currentData().toString();
    const QString iconTheme = iconCombo_->currentData().toString();
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("language"), lang);
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("theme"), theme);
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("iconTheme"), iconTheme);
    origLang_ = lang;
    origTheme_ = theme;
    origIconTheme_ = iconTheme;
}

// ============ PanelSettingsPage ============

PanelSettingsPage::PanelSettingsPage(QObject *parent)
    : QObject(parent) {
    widget_ = new QWidget;

    auto *layout = new QVBoxLayout(widget_);

    // 布局切换
    auto *orientBox = new QGroupBox(tr("Layout"));
    auto *orientLayout = new QVBoxLayout(orientBox);
    horizontalRadio_ = new QRadioButton(tr("Left / Right"));
    verticalRadio_ = new QRadioButton(tr("Top / Bottom"));
    orientLayout->addWidget(horizontalRadio_);
    orientLayout->addWidget(verticalRadio_);
    layout->addWidget(orientBox);

    // 显示面板
    auto *visibleBox = new QGroupBox(tr("Show Panels"));
    auto *visibleLayout = new QVBoxLayout(visibleBox);
    panel1VisibleCheck_ = new QCheckBox(tr("Show Panel 1"));
    panel2VisibleCheck_ = new QCheckBox(tr("Show Panel 2"));
    visibleLayout->addWidget(panel1VisibleCheck_);
    visibleLayout->addWidget(panel2VisibleCheck_);
    layout->addWidget(visibleBox);

    layout->addStretch(1);

    // 至少保持一个面板可见：禁用最后一个的取消
    connect(panel1VisibleCheck_, &QCheckBox::toggled, this,
            &PanelSettingsPage::onPanelVisibilityChanged);
    connect(panel2VisibleCheck_, &QCheckBox::toggled, this,
            &PanelSettingsPage::onPanelVisibilityChanged);
}

QString PanelSettingsPage::title() const { return tr("Panels"); }

void PanelSettingsPage::load() {
    auto *cfg = ConfigManager::instance();
    origOrientation_ = cfg->value(QStringLiteral("Panels"), QStringLiteral("orientation"),
                                    static_cast<int>(Qt::Horizontal)).toInt();
    origPanel1Visible_ = cfg->value(QStringLiteral("Panels"), QStringLiteral("panel1Visible"), true).toBool();
    origPanel2Visible_ = cfg->value(QStringLiteral("Panels"), QStringLiteral("panel2Visible"), true).toBool();

    (origOrientation_ == Qt::Horizontal ? horizontalRadio_ : verticalRadio_)->setChecked(true);
    panel1VisibleCheck_->setChecked(origPanel1Visible_);
    panel2VisibleCheck_->setChecked(origPanel2Visible_);
    onPanelVisibilityChanged();
}

void PanelSettingsPage::apply() {
    auto *cfg = ConfigManager::instance();
    const int orient = horizontalRadio_->isChecked() ? Qt::Horizontal : Qt::Vertical;
    const bool p1Visible = panel1VisibleCheck_->isChecked();
    const bool p2Visible = panel2VisibleCheck_->isChecked();

    cfg->setValue(QStringLiteral("Panels"), QStringLiteral("orientation"), orient);
    cfg->setValue(QStringLiteral("Panels"), QStringLiteral("panel1Visible"), p1Visible);
    cfg->setValue(QStringLiteral("Panels"), QStringLiteral("panel2Visible"), p2Visible);

    origOrientation_ = orient;
    origPanel1Visible_ = p1Visible;
    origPanel2Visible_ = p2Visible;
}

void PanelSettingsPage::onPanelVisibilityChanged() {
    // 至少保持一个面板显示：禁用最后一个的取消操作
    if (panel1VisibleCheck_->isChecked() && !panel2VisibleCheck_->isChecked()) {
        panel1VisibleCheck_->setEnabled(false);
    } else if (!panel1VisibleCheck_->isChecked() && panel2VisibleCheck_->isChecked()) {
        panel2VisibleCheck_->setEnabled(false);
    } else {
        panel1VisibleCheck_->setEnabled(true);
        panel2VisibleCheck_->setEnabled(true);
    }
}

// ============ FileBrowserSettingsPage ============

FileBrowserSettingsPage::FileBrowserSettingsPage(QObject *parent)
    : QObject(parent) {
    widget_ = new QWidget;

    auto *layout = new QVBoxLayout(widget_);

    // 显示隐藏文件
    showHiddenCheck_ = new QCheckBox(tr("Show hidden files (. prefix)"));
    layout->addWidget(showHiddenCheck_);

    // 日期时间格式
    auto *dtBox = new QGroupBox(tr("Date/Time Format"));
    auto *dtLayout = new QVBoxLayout(dtBox);
    dateTimeFormatEdit_ = new QLineEdit;
    dateTimeFormatEdit_->setPlaceholderText(QStringLiteral("yyyy-MM-dd HH:mm"));
    dtLayout->addWidget(dateTimeFormatEdit_);

    // 占位符说明
    auto *hint = new QLabel(
        tr("<b>Format placeholders</b> (Qt date/time format):<br>"
           "<table cellpadding='2'>"
           "<tr><td><b>yyyy</b></td><td>4-digit year (2026)</td>"
               "<td><b>yy</b></td><td>2-digit year (26)</td></tr>"
           "<tr><td><b>MM</b></td><td>Month, 2 digits (01-12)</td>"
               "<td><b>M</b></td><td>Month, no leading zero (1-12)</td></tr>"
           "<tr><td><b>MMM</b></td><td>Short month name (Jan)</td>"
               "<td><b>MMMM</b></td><td>Long month name (January)</td></tr>"
           "<tr><td><b>dd</b></td><td>Day, 2 digits (01-31)</td>"
               "<td><b>d</b></td><td>Day, no leading zero (1-31)</td></tr>"
           "<tr><td><b>ddd</b></td><td>Short day name (Mon)</td>"
               "<td><b>dddd</b></td><td>Long day name (Monday)</td></tr>"
           "<tr><td><b>HH</b></td><td>Hour 24h, 2 digits (00-23)</td>"
               "<td><b>H</b></td><td>Hour 24h, no leading zero (0-23)</td></tr>"
           "<tr><td><b>hh</b></td><td>Hour 12h, 2 digits (01-12)</td>"
               "<td><b>h</b></td><td>Hour 12h, no leading zero (1-12)</td></tr>"
           "<tr><td><b>mm</b></td><td>Minute, 2 digits (00-59)</td>"
               "<td><b>m</b></td><td>Minute, no leading zero (0-59)</td></tr>"
           "<tr><td><b>ss</b></td><td>Second, 2 digits (00-59)</td>"
               "<td><b>s</b></td><td>Second, no leading zero (0-59)</td></tr>"
           "<tr><td><b>ap</b></td><td>am/pm (lowercase)</td>"
               "<td><b>AP</b></td><td>AM/PM (uppercase)</td></tr>"
           "<tr><td><b>zzz</b></td><td>Milliseconds (000-999)</td>"
               "<td><b>z</b></td><td>Milliseconds, no leading zero</td></tr>"
           "</table>"
           "<br>Leave empty to use the default <i>yyyy-MM-dd HH:mm</i>.<br>"
           "Examples: <code>yyyy-MM-dd</code>, <code>yyyy/MM/dd HH:mm</code>, "
           "<code>MMM d, yyyy h:mm AP</code>"));
    hint->setTextFormat(Qt::RichText);
    hint->setWordWrap(true);
    dtLayout->addWidget(hint);
    layout->addWidget(dtBox);

    // 列选择
    auto *colsBox = new QGroupBox(tr("Visible Columns"));
    auto *colsLayout = new QVBoxLayout(colsBox);
    columnList_ = new QListWidget;
    columnList_->setSelectionMode(QAbstractItemView::NoSelection);
    colsLayout->addWidget(columnList_);
    layout->addWidget(colsBox, 1);
}

QString FileBrowserSettingsPage::title() const { return tr("File Browser"); }

void FileBrowserSettingsPage::load() {
    auto *cfg = ConfigManager::instance();
    origShowHidden_ = cfg->value(QStringLiteral("File_Browser"), QStringLiteral("showHidden"), false).toBool();
    showHiddenCheck_->setChecked(origShowHidden_);

    origDateTimeFormat_ = cfg->value(QStringLiteral("File_Browser"),
                                        QStringLiteral("dateTimeFormat"),
                                        QStringLiteral("yyyy-MM-dd HH:mm")).toString();
    dateTimeFormatEdit_->setText(origDateTimeFormat_);

    // 列选择
    const QStringList allCols = ColumnManager::instance()->allColumnNames();
    const QStringList currentOrder = ColumnManager::instance()->columnOrder();
    origVisibleColumns_ = currentOrder;

    columnList_->clear();
    for (const QString &name : allCols) {
        auto *item = new QListWidgetItem(name, columnList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(currentOrder.contains(name) ? Qt::Checked : Qt::Unchecked);
    }
}

void FileBrowserSettingsPage::apply() {
    auto *cfg = ConfigManager::instance();
    const bool showHidden = showHiddenCheck_->isChecked();
    cfg->setValue(QStringLiteral("File_Browser"), QStringLiteral("showHidden"), showHidden);
    origShowHidden_ = showHidden;

    // 日期时间格式：空值视为使用默认
    QString dtFmt = dateTimeFormatEdit_->text().trimmed();
    if (dtFmt.isEmpty()) dtFmt = QStringLiteral("yyyy-MM-dd HH:mm");
    cfg->setValue(QStringLiteral("File_Browser"), QStringLiteral("dateTimeFormat"), dtFmt);
    origDateTimeFormat_ = dtFmt;

    // 应用列选择：按列表顺序收集选中项
    QStringList visibleCols;
    for (int i = 0; i < columnList_->count(); ++i) {
        auto *item = columnList_->item(i);
        if (item->checkState() == Qt::Checked) {
            visibleCols << item->text();
        }
    }

    auto *cm = ColumnManager::instance();
    // 更新可见性与顺序
    const QStringList allNames = cm->allColumnNames();
    for (const QString &name : allNames) {
        cm->setColumnVisible(name, visibleCols.contains(name));
    }
    cm->setColumnOrder(visibleCols);
    origVisibleColumns_ = visibleCols;
}

// ============ ShortcutSettingsPage ============

ShortcutSettingsPage::ShortcutSettingsPage(QObject *parent)
    : QObject(parent) {
    widget_ = new QWidget;

    auto *layout = new QVBoxLayout(widget_);

    table_ = new QTableWidget;
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({tr("Action"), tr("Shortcut"), tr("Conflict")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    layout->addWidget(table_, 1);

    // 提示
    auto *hint = new QLabel(tr("Double-click a shortcut cell to edit. Conflicting items shown in red."));
    layout->addWidget(hint);
}

QString ShortcutSettingsPage::title() const { return tr("Shortcuts"); }

void ShortcutSettingsPage::load() {
    const QList<ShortcutItem> items = ShortcutManager::instance()->allShortcuts();

    // 定义显示顺序（按相关性分组）
    static const QStringList kDisplayOrder = {
        // 文件/标签页操作
        QStringLiteral("file.new_tab"),
        QStringLiteral("file.close_tab"),
        QStringLiteral("file.clone_tab"),
        QStringLiteral("file.new_file"),
        QStringLiteral("file.new_folder"),
        // 导航
        QStringLiteral("filelist.back"),
        QStringLiteral("filelist.forward"),
        QStringLiteral("filelist.up"),
        QStringLiteral("filelist.refresh"),
        QStringLiteral("nav.focus_panel"),
        // 文件操作
        QStringLiteral("filelist.open"),
        QStringLiteral("filelist.open_with"),
        QStringLiteral("filelist.rename"),
        QStringLiteral("filelist.cut"),
        QStringLiteral("filelist.copy"),
        QStringLiteral("filelist.paste"),
        QStringLiteral("filelist.cut_to_opposite"),
        QStringLiteral("filelist.copy_to_opposite"),
        QStringLiteral("filelist.copy_path"),
        QStringLiteral("filelist.copy_name"),
        // 删除
        QStringLiteral("filelist.trash"),
        QStringLiteral("filelist.delete"),
        // 属性
        QStringLiteral("filelist.properties"),
        // 视图/面板
        QStringLiteral("settings.toggle_hidden"),
        QStringLiteral("settings.switch_active_panel"),
        QStringLiteral("settings.toggle_orientation"),
        QStringLiteral("settings.toggle_panel1"),
        QStringLiteral("settings.toggle_panel2"),
        // 其他
        QStringLiteral("file.quit"),
        QStringLiteral("help.about"),
        // 选项卡上下文菜单
        QStringLiteral("tab.close"),
        QStringLiteral("tab.close_others"),
        QStringLiteral("tab.clone"),
    };

    // 按 kDisplayOrder 排序，未列入的追加到末尾
    QHash<QString, ShortcutItem> itemMap;
    for (const auto &item : items) {
        itemMap.insert(item.id, item);
    }
    QList<ShortcutItem> orderedItems;
    QSet<QString> added;
    for (const QString &id : kDisplayOrder) {
        auto it = itemMap.find(id);
        if (it != itemMap.end()) {
            orderedItems.append(it.value());
            added.insert(id);
        }
    }
    for (const auto &item : items) {
        if (!added.contains(item.id)) {
            orderedItems.append(item);
        }
    }

    table_->setRowCount(orderedItems.size());
    editedShortcuts_.clear();

    for (int i = 0; i < orderedItems.size(); ++i) {
        const ShortcutItem &item = orderedItems.at(i);

        // 动作名
        auto *nameItem = new QTableWidgetItem(item.displayText);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, item.id);
        table_->setItem(i, 0, nameItem);

        // 快捷键（可双击编辑）
        const QString keyStr = item.currentKey.isEmpty() ? item.defaultKey : item.currentKey;
        auto *keyItem = new QTableWidgetItem(keyStr);
        // 使用 QKeySequenceEdit 作为编辑器
        table_->setItem(i, 1, keyItem);

        // 冲突状态
        auto *conflictItem = new QTableWidgetItem(item.conflicted ? tr("Yes") : tr(""));
        conflictItem->setFlags(conflictItem->flags() & ~Qt::ItemIsEditable);
        table_->setItem(i, 2, conflictItem);

        editedShortcuts_[i] = keyStr;
    }

    refreshConflictHighlight();

    // 连接 cellChanged 以更新内存中的编辑
    connect(table_, &QTableWidget::cellChanged, this, &ShortcutSettingsPage::onCellChanged);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &ShortcutSettingsPage::onItemDoubleClicked);
}

void ShortcutSettingsPage::apply() {
    auto *sm = ShortcutManager::instance();
    for (int i = 0; i < table_->rowCount(); ++i) {
        auto *nameItem = table_->item(i, 0);
        if (!nameItem) continue;
        const QString id = nameItem->data(Qt::UserRole).toString();
        const QString keyStr = editedShortcuts_.value(i);
        sm->setShortcut(id, QKeySequence(keyStr));
    }
    sm->saveToConfig();
    sm->detectConflicts();
    refreshConflictHighlight();
}

void ShortcutSettingsPage::onCellChanged(int row, int column) {
    if (column != 1) return;
    auto *keyItem = table_->item(row, column);
    if (!keyItem) return;
    editedShortcuts_[row] = keyItem->text();
}

void ShortcutSettingsPage::onItemDoubleClicked(int row, int column) {
    if (column != 1) return;
    // 弹出 QKeySequenceEdit 对话框获取新快捷键
    QDialog dlg(widget_);
    dlg.setWindowTitle(tr("Edit Shortcut"));
    auto *layout = new QVBoxLayout(&dlg);
    auto *label = new QLabel(tr("Press a new key sequence:"));
    auto *edit = new QKeySequenceEdit;
    auto *keyItem = table_->item(row, 1);
    if (keyItem) edit->setKeySequence(QKeySequence(keyItem->text()));
    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(label);
    layout->addWidget(edit);
    layout->addWidget(btnBox);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const QString newKey = edit->keySequence().toString();
        if (keyItem) keyItem->setText(newKey);
        editedShortcuts_[row] = newKey;
    }
}

void ShortcutSettingsPage::refreshConflictHighlight() {
    // 收集所有当前快捷键，标记冲突项
    QMap<QString, QList<int>> keyToRows;
    for (int i = 0; i < table_->rowCount(); ++i) {
        const QString key = editedShortcuts_.value(i);
        if (key.isEmpty()) continue;
        keyToRows[key].append(i);
    }

    // 重置颜色
    const QBrush normalBrush = table_->palette().text();
    const QBrush conflictBrush(Qt::red);

    for (int i = 0; i < table_->rowCount(); ++i) {
        auto *nameItem = table_->item(i, 0);
        auto *keyItem = table_->item(i, 1);
        auto *conflictItem = table_->item(i, 2);
        if (!nameItem || !keyItem || !conflictItem) continue;

        const QString key = editedShortcuts_.value(i);
        const bool conflict = !key.isEmpty() && keyToRows.value(key).size() > 1;
        if (conflict) {
            nameItem->setForeground(conflictBrush);
            keyItem->setForeground(conflictBrush);
            conflictItem->setText(tr("Yes"));
            conflictItem->setForeground(conflictBrush);
        } else {
            nameItem->setForeground(normalBrush);
            keyItem->setForeground(normalBrush);
            conflictItem->setText({});
        }
    }
}

} // namespace fm
