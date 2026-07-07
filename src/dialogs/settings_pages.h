#ifndef FM_DIALOGS_SETTINGS_PAGES_H
#define FM_DIALOGS_SETTINGS_PAGES_H

#include "isettings_page.h"

#include <QWidget>

class QComboBox;
class QRadioButton;
class QCheckBox;
class QButtonGroup;
class QListWidget;
class QTableWidget;
class QTableWidgetItem;
class QKeySequenceEdit;

namespace fm {

// === 界面设置页：语言 + 主题 ===
class UiSettingsPage : public QObject, public ISettingsPage {
    Q_OBJECT
public:
    explicit UiSettingsPage(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("ui"); }
    QString title() const override;
    QWidget *widget() override { return widget_; }
    void load() override;
    void apply() override;
    void reset() override { load(); }

private:
    QWidget *widget_ = nullptr;
    QComboBox *langCombo_ = nullptr;
    QComboBox *themeCombo_ = nullptr;
    QString origLang_;
    QString origTheme_;
};

// === 面板设置页：活动面板 + 布局 + 显示面板 ===
class PanelSettingsPage : public QObject, public ISettingsPage {
    Q_OBJECT
public:
    explicit PanelSettingsPage(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("panel"); }
    QString title() const override;
    QWidget *widget() override { return widget_; }
    void load() override;
    void apply() override;
    void reset() override { load(); }

private slots:
    void onPanelVisibilityChanged();

private:
    QWidget *widget_ = nullptr;
    QRadioButton *panel1Radio_ = nullptr;
    QRadioButton *panel2Radio_ = nullptr;
    QRadioButton *horizontalRadio_ = nullptr;
    QRadioButton *verticalRadio_ = nullptr;
    QCheckBox *panel1VisibleCheck_ = nullptr;
    QCheckBox *panel2VisibleCheck_ = nullptr;

    int origActivePanel_ = 0;
    int origOrientation_ = 0;
    bool origPanel1Visible_ = true;
    bool origPanel2Visible_ = true;
};

// === 文件浏览设置页：显示隐藏文件 + 列设置 ===
class FileBrowserSettingsPage : public QObject, public ISettingsPage {
    Q_OBJECT
public:
    explicit FileBrowserSettingsPage(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("filebrowser"); }
    QString title() const override;
    QWidget *widget() override { return widget_; }
    void load() override;
    void apply() override;
    void reset() override { load(); }

private:
    QWidget *widget_ = nullptr;
    QCheckBox *showHiddenCheck_ = nullptr;
    QListWidget *columnList_ = nullptr;

    bool origShowHidden_ = false;
    QStringList origVisibleColumns_;
};

// === 快捷键设置页：表格 + QKeySequenceEdit ===
class ShortcutSettingsPage : public QObject, public ISettingsPage {
    Q_OBJECT
public:
    explicit ShortcutSettingsPage(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("shortcuts"); }
    QString title() const override;
    QWidget *widget() override { return widget_; }
    void load() override;
    void apply() override;
    void reset() override { load(); }

private slots:
    void onCellChanged(int row, int column);
    void onItemDoubleClicked(int row, int column);

private:
    void refreshConflictHighlight();

    QWidget *widget_ = nullptr;
    QTableWidget *table_ = nullptr;
    // 临时保存：row -> 当前编辑的快捷键
    QMap<int, QString> editedShortcuts_;
};

} // namespace fm

#endif // FM_DIALOGS_SETTINGS_PAGES_H
