#include "settings_pages.h"

#include "../core/column_manager.h"
#include "../core/config_manager.h"
#include "../core/shortcut_manager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
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

    layout->addStretch(1);
}

QString UiSettingsPage::title() const { return tr("Interface"); }

void UiSettingsPage::load() {
    auto *cfg = ConfigManager::instance();
    origLang_ = cfg->value(QStringLiteral("UI"), QStringLiteral("language"),
                            QStringLiteral("en")).toString();
    origTheme_ = cfg->value(QStringLiteral("UI"), QStringLiteral("theme"),
                             QStringLiteral("Fusion")).toString();

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
}

void UiSettingsPage::apply() {
    auto *cfg = ConfigManager::instance();
    const QString lang = langCombo_->currentData().toString();
    const QString theme = themeCombo_->currentData().toString();
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("language"), lang);
    cfg->setValue(QStringLiteral("UI"), QStringLiteral("theme"), theme);
    origLang_ = lang;
    origTheme_ = theme;
}

// ============ PanelSettingsPage ============

PanelSettingsPage::PanelSettingsPage(QObject *parent)
    : QObject(parent) {
    widget_ = new QWidget;

    auto *layout = new QVBoxLayout(widget_);

    // 活动面板
    auto *activeBox = new QGroupBox(tr("Active Panel"));
    auto *activeLayout = new QVBoxLayout(activeBox);
    panel1Radio_ = new QRadioButton(tr("Panel 1"));
    panel2Radio_ = new QRadioButton(tr("Panel 2"));
    activeLayout->addWidget(panel1Radio_);
    activeLayout->addWidget(panel2Radio_);
    layout->addWidget(activeBox);

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
    origActivePanel_ = cfg->value(QStringLiteral("Panels"), QStringLiteral("activePanel"), 0).toInt();
    origOrientation_ = cfg->value(QStringLiteral("Panels"), QStringLiteral("orientation"),
                                    static_cast<int>(Qt::Horizontal)).toInt();
    origPanel1Visible_ = cfg->value(QStringLiteral("Panels"), QStringLiteral("panel1Visible"), true).toBool();
    origPanel2Visible_ = cfg->value(QStringLiteral("Panels"), QStringLiteral("panel2Visible"), true).toBool();

    (origActivePanel_ == 0 ? panel1Radio_ : panel2Radio_)->setChecked(true);
    (origOrientation_ == Qt::Horizontal ? horizontalRadio_ : verticalRadio_)->setChecked(true);
    panel1VisibleCheck_->setChecked(origPanel1Visible_);
    panel2VisibleCheck_->setChecked(origPanel2Visible_);
    onPanelVisibilityChanged();
}

void PanelSettingsPage::apply() {
    auto *cfg = ConfigManager::instance();
    const int active = panel1Radio_->isChecked() ? 0 : 1;
    const int orient = horizontalRadio_->isChecked() ? Qt::Horizontal : Qt::Vertical;
    const bool p1Visible = panel1VisibleCheck_->isChecked();
    const bool p2Visible = panel2VisibleCheck_->isChecked();

    cfg->setValue(QStringLiteral("Panels"), QStringLiteral("activePanel"), active);
    cfg->setValue(QStringLiteral("Panels"), QStringLiteral("orientation"), orient);
    cfg->setValue(QStringLiteral("Panels"), QStringLiteral("panel1Visible"), p1Visible);
    cfg->setValue(QStringLiteral("Panels"), QStringLiteral("panel2Visible"), p2Visible);

    origActivePanel_ = active;
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
    table_->setRowCount(items.size());
    editedShortcuts_.clear();

    for (int i = 0; i < items.size(); ++i) {
        const ShortcutItem &item = items.at(i);

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
