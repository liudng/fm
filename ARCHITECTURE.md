# fm 架构设计文档

## 1. 架构概览

### 1.1 设计原则
- **模块化**：按职责划分模块，模块间通过明确接口通信
- **关注点分离**：UI、业务逻辑、数据访问分层
- **信号驱动**：利用 Qt 信号槽机制实现松耦合
- **配置驱动**：所有可配置项集中管理，运行时动态生效
- **异步优先**：耗时文件操作使用 Qt Concurrent + 信号回报进度

### 1.2 模块总览

```
┌─────────────────────────────────────────────────────────────┐
│                      app (应用入口)                          │
│           main / FmApplication / SingleInstance             │
└───────────────────────────┬─────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
┌─────────────────┐  ┌──────────────┐  ┌─────────────────┐
│   ui (界面)     │  │ core (核心)  │  │  fileops (操作) │
│ MainWindow      │  │ ConfigManager│  │ FileOperations  │
│ (内联菜单栏/    │  │ SessionState │  │  (内联异步作业) │
│  工具栏/卷菜单) │  │ FavoriteMgr  │  │ ConflictDialog  │
└────────┬────────┘  │ ShortcutMgr  │  │ ProgressDialog  │
         │           │ ClipboardMgr │  │ TrashCan        │
         │           │ ColumnMgr    │  └────────┬────────┘
         │           │ OpenWithMgr  │           │
         │           │ VolumeMgr    │           │
         │           │  (QStorage+  │           │
         │           │   UDisks2)   │           │
         │           └──────┬───────┘           │
         │                  │                   │
         ▼                  ▼                   ▼
┌─────────────────────────────────────────────────────────┐
│                  panel / filelist                        │
│  PanelContainer / PanelWidget / FileTabBar              │
│  FileListView / FileListModel(15列) / FileListSortProxy │
│  FileItem (含扩展属性) / ColumnManager                  │
└─────────────────────────┬───────────────────────────────┘
                          │
                          ▼
┌────────────────────────────────────────────────────────────┐
│                     dialogs                                │
│  SettingsDialog / 5 SettingsPages (含 FileOperationsPage) │
│  PropertiesDialog (3 分组 + ACL/ext flags)                 │
│  OpenWithDialog / ConflictDialog / AboutDialog / ...      │
└────────────────────────────────────────────────────────────┘
```
- **没有独立的 `volume/` 与 `settings/` 目录**：VolumeManager 在 `core/`，SettingsDialog 与 SettingsPages 在 `dialogs/`。
- **没有独立的 MenuBarManager / ToolBarManager / AbstractJob / CopyMoveJob 类**：菜单栏与工具栏构建在 MainWindow 内联，异步作业在 FileOperations 内联。

---

## 2. 目录结构

```
fm-qt/
├── CMakeLists.txt                     # 构建脚本（含 man/desktop/icon 安装）
├── REQUIREMENTS.md                    # 需求规格
├── ARCHITECTURE.md                    # 架构设计文档
├── README.md                           # 英文 README
├── README-zh.md                       # 中文 README
├── fm.1                               # man 手册（section 1）
├── fm.desktop                          # .desktop 文件
├── fm.png                             # 应用图标（512x512）
├── resources.qrc                      # Qt 资源文件（嵌入翻译 .qm）
├── .github/workflows/                 # CI/CD（build-image / ci / release）
├── docker/                            # 构建 Dockerfile（Debian/Fedora）
├── src/
│   ├── main.cpp                      # 程序入口
│   ├── app/
│   │   ├── fm_application.h           # QApplication 子类（图标主题/翻译）
│   │   ├── fm_application.cpp
│   │   ├── single_instance.h         # 单实例管理（QLocalServer）
│   │   └── single_instance.cpp
│   ├── core/
│   │   ├── config_manager.h          # 配置文件管理（QSettings 封装）
│   │   ├── config_manager.cpp
│   │   ├── session_state.h           # 会话状态（布局/选项卡/排序）
│   │   ├── session_state.cpp
│   │   ├── favorite_manager.h        # 收藏项管理
│   │   ├── favorite_manager.cpp
│   │   ├── shortcut_manager.h        # 快捷键管理（QPointer 跟踪绑定）
│   │   ├── shortcut_manager.cpp
│   │   ├── clipboard_manager.h       # 剪贴板管理（URI/剪切标记）
│   │   ├── clipboard_manager.cpp
│   │   ├── column_manager.h          # 列可见性/顺序/列宽（像素）管理
│   │   ├── column_manager.cpp
│   │   ├── open_with_manager.h       # "打开方式" MIME→应用映射持久化
│   │   ├── open_with_manager.cpp
│   │   ├── volume_manager.h          # QStorageInfo + UDisks2 D-Bus 挂载/卸载/弹出
│   │   └── volume_manager.cpp
│   ├── ui/
│   │   ├── main_window.h             # 主窗口（含菜单栏/工具栏/卷菜单内联实现）
│   │   └── main_window.cpp
│   ├── panel/
│   │   ├── panel_container.h         # 双面板容器（QSplitter/布局/显隐）
│   │   ├── panel_container.cpp
│   │   ├── panel_widget.h            # 单个面板（选项卡栏 + 文件列表栈）
│   │   ├── panel_widget.cpp
│   │   ├── file_tab_bar.h            # 选项卡栏（"+"/拖拽/右键菜单）
│   │   ├── file_tab_bar.cpp
│   │   └── panel_id.h                # PanelId 枚举（Panel1/Panel2）
│   ├── filelist/
│   │   ├── file_list_view.h          # QTreeView 子类
│   │   ├── file_list_view.cpp
│   │   ├── file_list_model.h         # QAbstractItemModel（15 列）
│   │   ├── file_list_model.cpp
│   │   ├── file_list_sort_proxy.h    # QSortFilterProxyModel（数值/日期排序）
│   │   ├── file_list_sort_proxy.cpp
│   │   ├── file_item.h               # 文件项数据结构（含扩展属性）
│   │   └── file_item.cpp
│   ├── fileops/
│   │   ├── file_operations.h         # 文件操作门面（含 QtConcurrent 异步作业）
│   │   ├── file_operations.cpp       # copyRecursively/copyFileChunked 等内联实现
│   │   ├── conflict_resolver.h       # ConflictResolution 枚举
│   │   ├── conflict_resolver.cpp
│   │   ├── progress_dialog.h         # 进度对话框（atomic<bool> 取消）
│   │   ├── progress_dialog.cpp
│   │   ├── trash_can.h               # FreeDesktop Trash 规范实现
│   │   └── trash_can.cpp
│   └── dialogs/
│       ├── isettings_page.h          # 设置页面接口
│       ├── settings_dialog.h         # 设置对话框主框架
│       ├── settings_dialog.cpp
│       ├── settings_pages.h          # 5 个设置页（含 FileOperationsSettingsPage）
│       ├── settings_pages.cpp
│       ├── about_dialog.h
│       ├── about_dialog.cpp
│       ├── properties_dialog.h       # 属性对话框（3 分组 + QProcess ACL/lsattr）
│       ├── properties_dialog.cpp
│       ├── open_with_dialog.h        # "打开..."选择应用
│       ├── open_with_dialog.cpp
│       ├── input_name_dialog.h       # 收藏新建/重名提示
│       ├── input_name_dialog.cpp
│       ├── conflict_dialog.h         # 同名冲突选择对话框
│       ├── conflict_dialog.cpp
│       ├── error_dialog.h             # 错误提示对话框
│       └── error_dialog.cpp
└── translations/
    ├── fm_en.ts                      # 英文翻译源
    ├── fm_zh.ts                      # 中文翻译源
    └── translate.py                  # 翻译辅助脚本（英→中映射）
```

> **说明**：实际代码中**没有** MenuBarManager / ToolBarManager / AbstractJob / CopyMoveJob / DeleteJob / TrashJob / FileListDelegate / NavigationHistory / udisks2_client 等类——菜单栏与工具栏构建、异步作业、冲突解决均直接内联于 MainWindow 与 FileOperations 中。文档早期版本按"理想分层"规划，实际实现采用了更扁平的结构。

---

## 3. 核心类设计

### 3.1 应用入口层

#### `FmApplication`（QApplication 子类）
**职责**：应用生命周期、事件过滤、全局翻译加载
```cpp
class FmApplication : public QApplication {
    Q_OBJECT
public:
    FmApplication(int &argc, char **argv);
    bool initialize();    // 初始化配置、翻译、单实例检测
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QTranslator translator_;
    SingleInstance *singleInstance_;
    ConfigManager *configManager_;
};
```

#### `SingleInstance`
**职责**：单实例检测与跨进程通信（QLocalServer/QLocalSocket）
```cpp
class SingleInstance : public QObject {
    Q_OBJECT
public:
    bool tryLock();        // 尝试获取锁，已存在则发送 path 给运行实例
    void sendPaths(const QStringList &paths);

signals:
    void pathsReceived(const QStringList &paths);  // 接收到新调用的 path
};
```

#### `CommandLineParser`（QCommandLineParser 封装）
**职责**：解析 `fm [path1] [path2]`，返回路径列表（转绝对路径）

---

### 3.2 核心配置层

#### `ConfigManager`（单例）
**职责**：封装 QSettings，提供 typed API，配置损坏检测与备份
```cpp
class ConfigManager : public QObject {
    Q_OBJECT
public:
    static ConfigManager *instance();

    // 通用读写（带默认值），setValue 同步写盘并 emit configChanged
    QVariant value(const QString &section, const QString &key,
                   const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &section, const QString &key, const QVariant &value);
    bool contains(const QString &section, const QString &key) const;

    // 错误处理
    bool load();                 // sync + 检测 status
    bool rebuild();              // 备份 + 重建默认配置
    void saveFailureRecover();   // 运行时保存失败：备份原文件，用内存配置重写
    bool backupCurrentFile(QString *backupPath = nullptr);
    QString filePath() const;

signals:
    void configChanged(const QString &section);  // 配置变更通知

private:
    QSettings *settings_;
    bool loaded_ = false;
};
```
- 默认配置通过 `ensureDefaultConfig()` 写入，包含 `[UI]`、`[Panels]`（含 `tabsClosable=false`）、`[File_Operations]`（`chunkSizeMB=1`）、`[File_Browser]`（`dateTimeFormat="yyyy-MM-dd HH:mm"`）、`[File_Browser_Columns]` 等默认值。
- 备份文件名格式：`config.ini.yyyyMMdd_HHmmss_zzz.bak`。

#### `SessionState`
**职责**：会话状态序列化/反序列化（双面板布局、比例、各选项卡路径、排序）
```cpp
struct TabState {
    QString path;
    int sortColumn;
    Qt::SortOrder sortOrder;
};

struct PanelState {
    QList<TabState> tabs;
    int activeTabIndex;
};

struct LayoutState {
    Qt::Orientation orientation;   // 横向=左右，纵向=上下
    QList<PanelState> panels;     // size==2
    QList<int> splitterSizes;     // 比例
    bool panelVisible[2];
};

class SessionState {
public:
    LayoutState toLayoutState() const;
    void fromLayoutState(const LayoutState &state);
    QByteArray serialize() const;     // 用于收藏项存储
    bool deserialize(const QByteArray &data);
};
```

#### `FavoriteManager`
**职责**：收藏项增删查改，名称 percent-encoding
```cpp
class FavoriteManager : public QObject {
    Q_OBJECT
public:
    QStringList listFavorites() const;
    LayoutState loadFavorite(const QString &name) const;
    bool saveFavorite(const QString &name, const LayoutState &state);
    bool deleteFavorite(const QString &name);
    bool exists(const QString &name) const;

signals:
    void favoritesChanged();

private:
    QString encodeName(const QString &name) const;   // percent-encoding
    QString decodeName(const QString &encoded) const;
};
```

#### `ShortcutManager`
**职责**：快捷键映射、冲突检测、应用到 QAction；使用 `QPointer<QAction>` 跟踪绑定，修改后通过 `reapplyShortcuts()` 立即生效
```cpp
struct ShortcutItem {
    QString id;            // 如 "filelist.refresh"
    QString defaultKey;    // 默认快捷键（如 "Ctrl+R"）
    QString currentKey;    // 用户配置
    QString displayText;   // 显示文本（用于设置页表格）
    bool conflicted;       // 是否冲突
};

class ShortcutManager : public QObject {
    Q_OBJECT
public:
    static ShortcutManager *instance();

    void initialize();                              // 加载默认值 + 配置 + 冲突检测
    QList<ShortcutItem> allShortcuts() const;
    QString shortcutFor(const QString &id) const;
    bool setShortcut(const QString &id, const QKeySequence &seq);
    void applyToAction(QAction *action, const QString &id);  // 绑定 QAction
    void reapplyShortcuts();                         // 配置变更后立即重应用
    void detectConflicts();                          // 仅第一个生效
    void saveToConfig();

private:
    QMap<QString, ShortcutItem> items_;
    QMap<QString, QPointer<QAction>> actionBindings_;  // id -> QAction（弱引用）
};
```
- 默认快捷键定义表涵盖：`file.*`（文件菜单）、`tab.*`（选项卡上下文）、`filelist.*`（右键菜单/键盘导航）、`settings.*`、`help.*`、`nav.focus_panel`（Ctrl+Tab 循环切换当前面板选项卡）。
- 关键默认值：`filelist.copy_to_opposite=F5`、`filelist.cut_to_opposite=F6`、`filelist.refresh=Ctrl+R`、`file.new_file=Ctrl+N`、`filelist.open_with=Ctrl+Shift+O`。`file.quit`/`filelist.copy_path`/`filelist.copy_name` 无默认快捷键。

#### `ClipboardManager`
**职责**：剪贴板内容管理，区分复制/剪切，URI 形式存储
```cpp
class ClipboardManager : public QObject {
    Q_OBJECT
public:
    enum Mode { Copy, Cut };

    void setFiles(const QList<QUrl> &urls, Mode mode);
    QList<QUrl> files() const;
    Mode mode() const;
    bool hasFiles() const;
    void clearCutMark(const QList<QUrl> &urls);   // 取消旧剪切项标记

signals:
    void clipboardChanged();

private:
    QList<QUrl> urls_;
    Mode mode_;
};
```

#### `OpenWithManager`（单例）
**职责**：持久化 `[OpenWith]` section 的 MIME 类型 → 应用映射；通过 `xdg-mime` 查询系统默认应用
```cpp
class OpenWithManager : public QObject {
    Q_OBJECT
public:
    static OpenWithManager *instance();

    // 查询"记住此选择"的应用 .desktop 路径（空表示未配置）
    QString defaultApplication(const QString &mimeType) const;
    // 设置 MIME 类型的默认应用（不修改系统关联）
    void setDefaultApplication(const QString &mimeType, const QString &desktopFile);
    // 通过 xdg-mime query default 查询系统默认应用
    static QString systemDefault(const QString &mimeType);
};
```
- `FileOperations::openWithDefault()` 先查 `OpenWithManager`，未配置时回退 `xdg-open`。
- `OpenWithDialog` 中的"记住此选择"复选框调用 `setDefaultApplication`。

---

### 3.3 主窗口层

#### `MainWindow`（QMainWindow）
**职责**：组装菜单栏、工具栏、双面板容器；**内联**实现菜单栏/工具栏构建、卷菜单刷新、收藏菜单刷新；处理关闭事件保存会话
```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    PanelContainer *panelContainer() const { return panelContainer_; }
    void addPathsToPanels(const QStringList &paths);   // 单实例接收到 path

protected:
    void closeEvent(QCloseEvent *event) override;       // 保存 Session 后退出
    bool eventFilter(QObject *obj, QEvent *event) override;  // 文件菜单卷项右键

private slots:
    void onExit();
    void onAddFavorite();
    void onFavoriteTriggered(const QString &name);
    void onToggleActivePanel();
    void onToggleOrientation();
    void onResetSplitter();
    void onTogglePanel1Visible();
    void onTogglePanel2Visible();
    void onToggleHiddenFiles();
    void onLanguageChanged(QAction *action);
    void onThemeChanged(QAction *action);
    void onOpenSettings();
    void onAbout();

private:
    void buildMenuBar();
    void buildFileMenu(QMenu *menu);                   // 三段：卷列表 / 外部设备 / 退出
    void buildFavoritesMenu(QMenu *menu);
    void buildSettingsMenu(QMenu *menu);
    void buildHelpMenu(QMenu *menu);
    void refreshFavoritesMenu();
    void refreshFileMenuVolumes();                      // aboutToShow 时实时枚举
    void refreshPanelActions();
    void updateToolbar();                              // 共享 PanelWidget 的 actions
    void restoreSession();
    void applyPanelConfig();
    void applyFileBrowserConfig();

    PanelContainer *panelContainer_ = nullptr;
    QToolBar *toolbar_ = nullptr;

    // 设置菜单中需要动态更新文字的项
    QAction *toggleOrientationAction_ = nullptr;
    QAction *togglePanel1Action_ = nullptr;
    QAction *togglePanel2Action_ = nullptr;
    QAction *toggleHiddenAction_ = nullptr;
    QMenu *favoritesMenu_ = nullptr;
    QMenu *languageMenu_ = nullptr;
    QMenu *themeMenu_ = nullptr;
    QMenu *fileMenu_ = nullptr;
    QAction *volSeparator_ = nullptr;     // 卷项与外部设备项之间的分隔符
    QAction *extSeparator_ = nullptr;     // 外部设备项与 Quit 之间的分隔符
    QList<QAction*> volActions_;          // 动态卷项（aboutToShow 时刷新）
    QList<QAction*> extActions_;          // 动态外部设备项（aboutToShow 时刷新）
    QActionGroup *languageGroup_ = nullptr;
    QActionGroup *themeGroup_ = nullptr;
};
```
- **没有独立 MenuBarManager / ToolBarManager 类**：菜单构建、卷刷新、收藏刷新、工具栏更新均在 MainWindow 内联实现。
- 工具栏通过 `PanelWidget::toolbarActions()` 获取共享 QAction 列表（已按组分隔），直接添加到 QToolBar。
- 文件菜单卷项的右键（卸载/弹出/挂载）通过 `eventFilter` 模式处理（参考收藏菜单右键删除）。
- 文件菜单 aboutToShow 时刷新；挂载/卸载/弹出操作后整体刷新文件菜单。

---

### 3.4 面板层

#### `PanelContainer`（QWidget）
**职责**：管理双面板布局、QSplitter、显示/隐藏、活动面板切换
```cpp
class PanelContainer : public QWidget {
    Q_OBJECT
public:
    PanelContainer(QWidget *parent = nullptr);

    PanelWidget *panel(PanelId id) const;
    PanelWidget *activePanel() const;
    void setActivePanel(PanelId id);

    void setOrientation(Qt::Orientation orientation);
    void setPanelVisible(PanelId id, bool visible);
    bool isPanelVisible(PanelId id) const;

    QList<int> splitterSizes() const;          // 比例
    void setSplitterSizes(const QList<int> &sizes);

signals:
    void activePanelChanged(PanelId id);
    void panelVisibilityChanged(PanelId id, bool visible);
    void panelVisibilityChanged();    // 触发"复制到对面"等操作启用状态刷新

private:
    QSplitter *splitter_;
    PanelWidget *panels_[2];
    PanelId activePanel_;
};
```

#### `PanelWidget`（QWidget）
**职责**：单个面板，包含选项卡栏 + 当前文件列表视图栈；管理每选项卡的导航历史；持久化 QAction 供工具栏/右键菜单共享
```cpp
struct TabData {
    FileListView *view = nullptr;
    FileListModel *model = nullptr;
    FileListSortProxy *proxy = nullptr;
    QList<QString> history;     // 导航历史（每选项卡独立）
    int historyIndex = -1;      // 当前历史位置
};

class PanelWidget : public QWidget {
    Q_OBJECT
public:
    PanelWidget(PanelId id, QWidget *parent = nullptr);

    PanelId id() const { return id_; }

    // 选项卡管理
    int addTab(const QString &path, int index = -1);     // 返回新选项卡索引
    void closeTab(int index);
    void closeOtherTabs(int index);
    int cloneTab(int index);
    int tabCount() const;
    int activeTabIndex() const;
    void setActiveTab(int index);
    QString tabPath(int index) const;
    QString activeTabPath() const;

    // 文件列表（活动选项卡）
    FileListView *listView() const;
    FileListModel *model() const;
    FileListSortProxy *proxyModel() const;
    FileTabBar *tabBar() const;

    // 会话状态（含每选项卡的排序列与方向）
    QList<TabState> tabStates() const;
    void setTabStates(const QList<TabState> &states, int activeIndex);

    // 选中项
    QList<FileItem> selectedItems() const;
    QList<QUrl> selectedUrls() const;
    bool hasSelection() const;
    bool hasSingleSelection() const;

    // 右键菜单
    void showContextMenu(const QPoint &globalPos, bool hasSelection);

    // 持久化 QAction（供工具栏与右键菜单共享，Qt::WidgetWithChildrenShortcut）
    QList<QAction*> toolbarActions() const;
    void updateActionStates();

    // 活动面板指示（粗体活动选项卡标题）
    void setActivePanel(bool active);

signals:
    void activeTabChanged(int index);
    void tabCountChanged();
    void pathChanged(const QString &path);
    void parentDirRequested();
    void openRequested(const QString &path);
    void contextMenuRequested(const QPoint &globalPos, bool hasSelection);
    void selectionChanged();

public slots:
    void navigateBack();
    void navigateForward();
    void navigateUp();
    void refresh();
    void openPath(const QString &path);   // 在活动选项卡中打开路径
    // 右键菜单/工具栏动作槽
    void onOpen();
    void onOpenWith();
    void onCopy();
    void onCut();
    void onPaste();
    void onCopyToOpposite();
    void onCutToOpposite();
    void onTrash();
    void onDeletePermanently();
    void onRename();
    void onProperties();
    void onCopyPath();
    void onCopyFileName();
    void onNewFile();
    void onNewFolder();

private:
    PanelId id_;
    bool isActivePanel_ = false;
    FileTabBar *tabBar_;
    QStackedWidget *stack_;        // 每个选项卡一个 FileListView
    QList<TabData> tabs_;          // 与选项卡栏顺序同步

    // 持久化 QAction（22 个，按工具栏分组顺序）
    QAction *actBack_, *actForward_, *actUp_, *actRefresh_;
    QAction *actNewFile_, *actNewFolder_;
    QAction *actOpen_, *actOpenWith_;
    QAction *actRename_, *actCut_, *actCopy_, *actPaste_;
    QAction *actCutToOpp_, *actCopyToOpp_, *actCopyPath_, *actCopyName_;
    QAction *actTrash_, *actDelete_, *actProperties_;
    QAction *actNewTab_, *actCloseTab_, *actCloneTab_, *actNextTab_;
};
```
- **选项卡拖拽同步**：监听 `QTabBar::tabMoved(from, to)`，同步 `tabs_` QList 与 QStackedWidget 中的 widget 位置（`stack_->insertWidget(to, w)` 重新定位已存在的 widget），并设置当前索引。
- **pathChanged 索引查找**：`FileListModel::pathChanged` 信号 lambda 不捕获固定 index（拖拽后会失效），而是按 model 指针动态查找 `tabs_` 中的当前索引。
- **配置变更监听**：监听 `ConfigManager::configChanged`，按 section 分发：
  - `Panels` → 应用 `tabsClosable` 到 tabBar
  - `File_Browser` → 应用 `showHidden` 与 `dateTimeFormat` 到所有 model
  - `File_Browser_Columns` → `ColumnManager::applyToView` 所有 view
- **面板激活**：成为活动面板时调用 `listView()->setFocus()` 确保方向键控制当前面板。
- **findContainer()**：向上遍历 `parentWidget()` 链查找 PanelContainer（因 QSplitter::addWidget 会重设父对象）。

#### `FileTabBar`（QTabBar 子类）
**职责**：选项卡标题（截断 16 字符）、tooltip、拖拽重排、"+" 新建按钮（手动绘制）、右键上下文菜单
```cpp
class FileTabBar : public QTabBar {
    Q_OBJECT
public:
    FileTabBar(QWidget *parent = nullptr);

    void setTabPath(int index, const QString &path);   // 自动设置标题（截断16字符）+ tooltip

signals:
    void newTabRequested();
    void closeTabRequested(int index);
    void contextMenuRequested(int index, const QPoint &pos);

protected:
    void paintEvent(QPaintEvent *event) override;        // 手动绘制 "+" 按钮
    void mousePressEvent(QMouseEvent *event) override;   // 检测 "+" 按钮点击
    bool isNewTabButton(const QPoint &pos) const;
};
```
- **没有 setCornerWidget**：QTabBar 不支持 `setCornerWidget`（仅 QTabWidget 支持），"+" 按钮通过 `paintEvent` 手动绘制 `QIcon::fromTheme("tab-new")`，在 `mousePressEvent` 中通过 `isNewTabButton()` 检测点击区域。
- **关闭按钮**：通过 `setTabsClosable()` 由 PanelWidget 控制（默认 false）。关闭按钮点击触发 `closeTabRequested` 信号。
- **拖拽**：使用 QTabBar 内置拖拽，触发 `tabMoved(from, to)` 信号（由 PanelWidget 监听并同步内容）。

---

### 3.5 文件列表层

#### `FileItem`（数据结构）
```cpp
struct FileItem {
    QString name;              // 文件全名
    QString absolutePath;
    qint64 size = 0;          // 字节（文件夹为 0）
    qint64 diskUsage = 0;     // 实际占用磁盘空间（st_blocks * 512）
    bool isDir = false;
    bool isSymLink = false;
    QString symLinkTarget;     // 符号链接直接目标（不做多层解析）
    QString mimeTypeName;      // image/png
    QString mimeTypeComment;  // PNG 图像
    QIcon icon;
    QString owner;
    QString group;
    uint ownerId = 0;          // 所有者 UID（st_uid）
    uint groupId = 0;          // 所属组 GID（st_gid）
    QDateTime created;
    QDateTime modified;
    QDateTime accessed;        // 最后访问时间（st_atime）
    QDateTime statusChanged;   // 状态变更时间（st_ctime）
    QFile::Permissions permissions;
    quint64 inode = 0;
};
```
- `loadDirectory()` 使用 `stat()` 系统调用填充 ownerId、groupId、diskUsage（`st_blocks * 512`）、accessed（`fi.lastRead()`）、statusChanged；`QFileInfo` 仅提供 created/modified 的近似。
- `Q_DECLARE_METATYPE(fm::FileItem)` 允许通过 `Qt::UserRole + 1` 在 model 中传递完整结构。

#### `FileListModel`（QAbstractItemModel）
**职责**：加载目录、提供 15 列行数据、管理 ".." 行
```cpp
class FileListModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column {
        ColIcon = 0, ColName, ColSize, ColType, ColMimeType,
        ColGroup, ColOwner,
        ColOwnerUid, ColGroupGid,                    // UID/GID（数字）
        ColCreated, ColModified,
        ColAccessed, ColDiskUsage, ColStatusChanged, // 访问/磁盘占用/状态变更
        ColPermissions,
        ColCount
    };
    Q_ENUM(Column)

    enum Role {
        FileItemRole = Qt::UserRole + 1,   // 返回完整 FileItem
        IsParentRowRole,                    // 是否 ".." 行
    };

    void setPath(const QString &path);
    QString path() const;
    void reload();

    bool showHidden() const;
    void setShowHidden(bool show);

    // 日期时间显示格式（空字符串表示使用 ISO 默认）
    QString dateTimeFormat() const;
    void setDateTimeFormat(const QString &format);   // 触发 dataChanged

    FileItem itemAt(const QModelIndex &index) const;
    bool isParentRow(const QModelIndex &index) const;

signals:
    void pathChanged(const QString &path);
    void loadError(const QString &errorMsg);    // 无权限/不存在

private:
    QString path_;
    QList<FileItem> items_;
    bool showHidden_ = false;
    bool hasParent_ = false;          // 是否显示 ".."
    QString dateTimeFormat_;          // 由 [File_Browser] dateTimeFormat 配置
};
```
- 默认排序列为 `ColName`（升序），由 PanelWidget 在 addTab 时通过 `view->header()->setSortIndicator(ColName, AscendingOrder)` 设置。
- 时间列的显示格式由 `dateTimeFormat_` 控制，`setDateTimeFormat()` 触发 `dataChanged` 信号通知所有时间列（ColCreated 至 ColStatusChanged）重绘。

#### `FileListSortProxy`（QSortFilterProxyModel）
**职责**：处理列头排序、次要排序（保留上次排序条件）、".."行始终位于首行；数值/日期列按实际值排序
```cpp
class FileListSortProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    int lastSortColumn() const;
    Qt::SortOrder lastSortOrder() const;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    int lastSortColumn_ = -1;        // 次要排序条件
    Qt::SortOrder lastSortOrder_ = Qt::AscendingOrder;
};
```
- **".."行处理**：lessThan 检测 `IsParentRowRole`，无论升序/降序均返回固定排序结果使其位于首行（降序时调用 `lessThan(b, a)`，需检查 `lastSortOrder_` 而非简单返回 true）。
- **数值列排序**：`ColSize`、`ColDiskUsage` 按 `qint64` 字节比较；`ColOwnerUid`、`ColGroupGid` 按数字比较；`ColInode` 按数值比较。避免按格式化字符串排序导致 "9 KB" > "10 MB"。
- **时间列排序**：`ColCreated`、`ColModified`、`ColAccessed`、`ColStatusChanged` 按 `QDateTime` 实际值比较。
- **次要排序**：当主排序列值相同时，使用 `lastSortColumn_` 与 `lastSortOrder_` 作为次要条件。

#### `FileListView`（QTreeView 子类）
**职责**：选择管理、右键菜单触发、键盘导航、双击/回车处理
```cpp
class FileListView : public QTreeView {
    Q_OBJECT
public:
    FileListView(QWidget *parent = nullptr);

    void setColumnConfig(const ColumnConfig &config);   // 可见性/顺序/列宽（像素）

signals:
    void openRequested(const QModelIndex &index);       // 双击/回车
    void openWithRequested(const QModelIndex &index);
    void renameRequested(const QModelIndex &index);
    void contextMenuRequested(const QPoint &pos, bool hasSelection);
    void selectionChangedSignal(const QItemSelection &selected);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;     // 键盘导航
    void contextMenuEvent(QContextMenuEvent *event) override;
};
```

#### `ColumnManager`（单例）
**职责**：列可见性、列顺序、列宽（像素）的全局管理与持久化；监听 QHeaderView 的 sectionResized/sectionMoved 信号实现"调整一处影响所有选项卡"
```cpp
class ColumnManager : public QObject {
    Q_OBJECT
public:
    static ColumnManager *instance();

    QStringList allColumnNames() const;                 // 15 列名（Icon/Name/Size/...）
    bool isColumnVisible(const QString &name) const;
    void setColumnVisible(const QString &name, bool visible);
    int columnWidth(const QString &name) const;
    void setColumnWidth(const QString &name, int width);
    QStringList columnOrder() const;
    void setColumnOrder(const QStringList &order);

    void registerView(FileListView *view);              // 监听 header 拖拽
    void unregisterView(FileListView *view);
    void applyToView(FileListView *view);                // 应用配置到单个 view
    void applyToAllViews();

    void loadFromConfig();
    void saveToConfig();

signals:
    void columnsChanged();

private:
    QList<FileListView*> views_;
    QMap<QString, bool> visibleMap_;
    QMap<QString, int> widthMap_;                       // 像素宽度
    QStringList order_;                                  // 可见列顺序
    bool applying_ = false;                              // 防止信号递归
};
```
- **列定义表** `kColumnDefs[]` 在 `column_manager.cpp` 中定义，包含 15 个列的名称、枚举值、默认像素宽度。Name 列默认宽度为 0（Stretch 模式）。
- **最小列宽** 20 像素。
- **Name 列特殊处理**：不存储宽度（Stretch 自动填充），`setColumnWidth` 与 `onSectionResized` 跳过 Name 列。
- **配置存储**：`[File_Browser_Columns] columns`（逗号分隔的可见列名顺序），`width_<列名>`（像素宽度）。
- **默认列**：`Icon,Name,Size,Modified`。

---

### 3.6 文件操作层

#### `FileOperations`（门面，单例）
**职责**：统一入口；通过 `QtConcurrent::run` 异步执行复制/移动/删除/回收站；在主线程预扫描阶段解决冲突；管理进度对话框
```cpp
class FileOperations : public QObject {
    Q_OBJECT
public:
    static FileOperations *instance();

    // 异步操作（QtConcurrent::run）
    void copy(const QList<QUrl> &sources, const QString &destDir);
    void move(const QList<QUrl> &sources, const QString &destDir);
    void trash(const QList<QUrl> &sources);
    void deletePermanently(const QList<QUrl> &sources);
    void pasteFromClipboard(const QString &destDir);

    // 同步操作（快速）
    void rename(const QUrl &target, const QString &newName);
    void createFile(const QString &dir, const QString &defaultName);
    void createDir(const QString &dir, const QString &defaultName);

    // 打开操作
    void openWithDefault(const QUrl &file);                 // 先查 OpenWithManager
    void openWithApplication(const QUrl &file, const QString &desktopFile);
    void openWithCommand(const QUrl &file, const QString &command);

signals:
    void directoryChanged(const QString &dir);   // 操作完成后刷新该目录
    void operationCompleted();
    void operationFailed(const QString &errorMsg);

private:
    void runCopyMove(const QList<QUrl> &sources, const QString &destDir, bool isMove);
    void runTrash(const QList<QUrl> &sources);
    void runDelete(const QList<QUrl> &sources);

    // 主线程同步调用（已在主线程，直接显示对话框，避免 BlockingQueuedConnection 死锁）
    ConflictResolution resolveConflict(const QString &sourceName,
                                        const QString &destPath, bool allowBatch);
    static QString uniqueName(const QString &dir, const QString &name);

    ConflictResolution batchResolution_ = ConflictResolution::Cancel;
    bool hasBatchResolution_ = false;
    ProgressDialog *progressDialog_;
};
```
- **没有独立的 Job 类**：`copyRecursively`、`copyFileChunked`、`removeRecursively` 等函数均在 `file_operations.cpp` 匿名命名空间内联实现。
- **预扫描冲突解决**：`runCopyMove` 在启动 QtConcurrent 之前，在主线程同步遍历所有源项，检查目标是否存在、是否为源目录或其子目录、调用 `resolveConflict` 弹对话框。预扫描完成后才创建 ProgressDialog 并启动异步作业。**原因**：若在 ConflictDialog::exec() 的本地事件循环中触发 ProgressDialog::showDelayed() 的定时器，进度对话框会覆盖冲突对话框导致无法操作。
- **分块复制**：`copyFileChunked` 从 `[File_Operations] chunkSizeMB` 读取分块大小（默认 1 MB），按块复制并在每块后通过 `QMetaObject::invokeMethod(progress, lambda, Qt::QueuedConnection)` 线程安全地更新进度。
- **取消处理**：`std::atomic<bool> canceled` 标志位；工作线程检测到取消时关闭文件句柄并 `QFile::remove(dst)` 删除未完成的不完整文件，已完成的文件不回滚。ProgressDialog::showDelayed() 的定时器在创建 ProgressDialog 后启动。
- **防止递归**：对每个源目录，使用 `canonicalPath()` 解析符号链接与相对路径，若目标目录等于源或为其子目录则跳过并弹错误对话框。

#### `ConflictResolution` 枚举与 `ConflictDialog`
```cpp
enum class ConflictResolution {
    Overwrite, Skip, Rename,
    OverwriteAll, SkipAll, RenameAll,
    Cancel   // 取消整个粘贴操作
};
```
- `ConflictDialog` 显示 6 个按钮（覆盖/跳过/重命名 + 全部变体）+ 取消按钮。
- `allowBatch` 参数控制是否显示"全部"按钮（多选时为 true）。
- `batchResolution_` 记忆"全部"模式，后续冲突直接返回，但 RenameAll 仍需弹对话框让用户输入新名。

#### `ProgressDialog`
**职责**：进度对话框，使用 `std::atomic<bool>` 取消标志；`showDelayed()` 通过定时器延迟显示（避免短任务闪烁）
```cpp
class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    void setProgress(int percent);
    void setCurrentFile(const QString &name);
    bool isCanceled() const;
    void showDelayed(int delayMs = 1000);   // 预估 >1s 才显示

signals:
    void canceled();
};
```

#### `TrashCan`
**职责**：实现 FreeDesktop.org Trash 规范，外部分区支持
```cpp
class TrashCan {
public:
    bool moveToTrash(const QList<QUrl> &files, QString *errorMsg);
    static QString trashDirForFile(const QUrl &file);   // ~/.local/share/Trash 或 .Trash-1000
};
```
- 外部分区优先使用 `.Trash/<uid>`（需满足安全条件：是目录、非符号链接、sticky bit 设置），不满足时回退 `.Trash-<uid>`。
- 同分区使用 `$XDG_DATA_HOME/Trash`（`~/.local/share/Trash`）。

---

### 3.7 卷管理层

#### `VolumeManager`（单例）
**职责**：通过 `QStorageInfo::mountedVolumes()` 枚举已挂载卷；通过 UDisks2 D-Bus 枚举外部（可移动）设备（含未挂载）；提供挂载/卸载/弹出操作
```cpp
struct VolumeInfo {
    QString devicePath;       // D-Bus 对象路径（/org/freedesktop/UDisks2/block_devices/sdb1）
    QString deviceFile;       // /dev/sdb1
    QString mountPoint;       // /media/user/USB（空表示未挂载）
    QString label;            // 卷标（IdLabel）
    QString fsType;           // ext4, vfat, ...
    QString mimeType;
    bool isMounted = false;
    bool isRemovable = false;
    bool isExternal = false;  // 外部设备（U 盘/SD 卡）
    QIcon icon;
};

class VolumeManager : public QObject {
    Q_OBJECT
public:
    static VolumeManager *instance();

    // 已挂载卷（QStorageInfo::mountedVolumes，含 btrfs 子卷的多个挂载点）
    QList<VolumeInfo> listVolumes();
    // 外部可移动设备（含未挂载，UDisks2 Manager.GetBlockDevices）
    QList<VolumeInfo> listExternalDevices();

    // 挂载/卸载/弹出（接受设备文件 /dev/sdXN，内部转换为 D-Bus 对象路径）
    QString mount(const QString &deviceFile, QString *errorMsg = nullptr);
    bool unmount(const QString &deviceFile, QString *errorMsg);
    bool eject(const QString &deviceFile, QString *errorMsg);

signals:
    void volumesChanged();

private:
    QStringList enumerateBlockDevices();
    VolumeInfo getBlockDeviceProperties(const QString &blockPath);
    QString toUDisks2ObjectPath(const QString &deviceFile);  // /dev/sdb1 → /org/freedesktop/UDisks2/block_devices/sdb1
};
```
- **两套枚举来源**：`listVolumes()` 使用 `QStorageInfo::mountedVolumes()`（更直接，含 btrfs 子卷）；`listExternalDevices()` 使用 UDisks2 `Manager.GetBlockDevices`，过滤 `Removable==true && IdUsage==filesystem`，包含未挂载设备。
- **UDisks2 D-Bus 调用要点**：
  - `Manager.GetBlockDevices` 签名是 `(IN a{sv} options, OUT ao)`，必须传 `QVariantMap{}` 作为参数，否则返回 "Invalid arguments" 错误。
  - `Filesystem.MountPoints`（aay 类型）需通过 `org.freedesktop.DBus.Properties.Get` 方法 + `QDBusArgument >>` 手动反序列化为 `QList<QByteArray>`；直接使用 `QDBusInterface::property()` 会触发 QDBusRawType 未注册警告。
  - `Device` 和 `MountPoints` 属性需用 `constData()` 截断尾部 null 字节，否则设备路径后出现方框字符导致挂载失败。
- **设备文件 → D-Bus 路径转换**：`/dev/sdb1` → `/org/freedesktop/UDisks2/block_devices/sdb1`，由 `toUDisks2ObjectPath()` 内部完成。
- **设备显示名称优先级**：`QStorageInfo::name()`（已挂载）→ `IdLabel` → `Drive.Model`；`deviceFile` 始终显示。

#### 文件菜单卷项（MainWindow 内联实现）
**职责**：文件菜单 aboutToShow 时通过 `VolumeManager::listVolumes()` 与 `listExternalDevices()` 枚举，动态添加为 QAction（三段：卷列表 / 外部设备列表 / 退出）。
```cpp
// MainWindow 私有成员
QAction *volSeparator_;          // 卷项与外部设备项之间的分隔符
QAction *extSeparator_;          // 外部设备项与 Quit 之间的分隔符
QList<QAction*> volActions_;     // 动态卷项（aboutToShow 时刷新）
QList<QAction*> extActions_;    // 动态外部设备项

// eventFilter 处理文件菜单卷项右键
// - 卷项右键：安全卸载 / 弹出
// - 外部设备项右键：未挂载→挂载；已挂载→安全卸载 / 弹出
```

---

### 3.8 设置对话框层

#### `SettingsDialog`
**职责**：左侧 sidebar + 右侧内容区，统一管理应用/取消/确定
```cpp
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(QWidget *parent = nullptr);
    void addPage(ISettingsPage *page);
    void showPage(const QString &pageId);   // "ui"/"panel"/"filebrowser"/"fileops"/"shortcuts"

private slots:
    void onApply();
    void onOk();
    void onCancel();

private:
    QList<ISettingsPage*> pages_;
    QStackedWidget *contentStack_;
    QListWidget *sidebar_;
};
```

#### `ISettingsPage`（接口）
```cpp
class ISettingsPage {
public:
    virtual ~ISettingsPage() = default;
    virtual QString id() const = 0;
    virtual QString title() const = 0;
    virtual QWidget *widget() = 0;
    virtual void load() = 0;        // 从 ConfigManager 加载
    virtual void apply() = 0;       // 保存到 ConfigManager
    virtual void reset() = 0;       // 恢复到上次 apply 的状态（默认实现为 load()）
};
```

#### 五个设置页（均在 `settings_pages.h/cpp` 中）

| 页面类 | id | 标题 | 关键控件 |
|---|---|---|---|
| `UiSettingsPage` | `ui` | Interface | 语言/主题/图标主题下拉框 |
| `PanelSettingsPage` | `panel` | Panels | 布局单选、显示面板复选框、启用选项卡关闭按钮复选框 |
| `FileBrowserSettingsPage` | `filebrowser` | File Browser | 显示隐藏文件复选框、日期时间格式输入框（含占位符说明）、可见列列表 |
| `FileOperationsSettingsPage` | `fileops` | File Operations | 粘贴分块大小 QSpinBox（1–64 MB，含推荐值说明） |
| `ShortcutSettingsPage` | `shortcuts` | Shortcuts | QTableWidget + QKeySequenceEdit，冲突项红色字体 |

- `ShortcutSettingsPage` 表格按相关性分组排列：文件/标签页操作 → 导航 → 文件操作 → 删除 → 属性 → 视图/面板 → 其他 → 选项卡上下文菜单。
- 双击快捷键单元格弹出 `QKeySequenceEdit` 对话框编辑；冲突检测通过 `keyToRows` 映射统计重复项。
- 已废弃的快捷键 id（`nav.focus_panel` 旧的 "Focus Tab Bar" / "Focus Panel"）在 .ts 中标记为 `type="vanished"`。

---

### 3.9 对话框层

#### `OpenWithDialog`
**职责**：通过 `xdg-mime query default <mime>` 与解析 `/usr/share/applications/*.desktop` 列出可用程序；提供"记住此选择"复选框
```cpp
class OpenWithDialog : public QDialog {
public:
    OpenWithDialog(const QString &mimeType, QWidget *parent);
    QString selectedDesktopFile() const;
    bool rememberChoice() const;
};
```

#### `PropertiesDialog`
**职责**：分组展示属性（3 个 QGroupBox），宽度约 750px；时间戳格式与文件列表配置一致
- **基本信息**：名称、路径、类型、MIME、大小、实际占用磁盘空间、创建时间、最后修改时间、最后访问时间
- **用户与权限**：所有者 UID、所有者、所属组 GID、所属组、权限、ACL 访问控制列表、状态变更时间
- **系统信息**：Inode、ext2/ext3/ext4/btrfs/xfs 标志位、符号链接目标（仅符号链接）
- ACL 通过 `QProcess` 调用 `getfacl -cp` 获取（去除注释行，3 秒超时，无 ACL 或不可用时显示"(无或不可用)"）。
- 标志位通过 `QProcess` 调用 `lsattr -d` 获取（不可用时显示"(不可用)"）。
- 属性值标签支持鼠标选中文本（`Qt::TextSelectableByMouse`）。
- 匿名命名空间中的 `formatDateTime()` 使用 `QObject::tr()` 包装错误信息。

#### `ConflictDialog`
**职责**：6 个按钮（覆盖/跳过/重命名 + 全部覆盖/全部跳过/全部重命名）+ 取消；多选时（`allowBatch=true`）显示"全部"按钮
```cpp
class ConflictDialog : public QDialog {
public:
    ConflictDialog(const QString &sourceName, const QString &destPath,
                   bool allowBatch, QWidget *parent);
    ConflictResolution resolution() const;
};
```
- 点击"取消"调用 `reject()`，`resolution_` 保持默认值 `Cancel`，触发上层取消整个粘贴操作。

---

## 4. 信号槽关系图

### 4.1 启动流程
```
main()
  └─> FmApplication::initialize()
        ├─> SingleInstance::tryLock()  ─失败─> sendPaths() ─> 退出
        ├─> ConfigManager::instance()->load()  ─损坏─> 提示重建/退出
        ├─> ShortcutManager::instance()->initialize()  // 加载默认值 + 配置 + 冲突检测
        ├─> ColumnManager::instance()->loadFromConfig()
        ├─> FmApplication::applyIconTheme()  // 从 [UI] iconTheme 读取
        ├─> QTranslator::load(defaultLang)   // 从 [UI] language 读取
        └─> MainWindow::show()  // 构建菜单/工具栏/双面板 + restoreSession()
```

### 4.2 文件列表导航
```
FileListView::openRequested(proxyIndex)
  └─> PanelWidget::onOpenRequested(proxyIndex)
        ├─> 若是文件夹 ─> PanelWidget::navigateTo(path, addHistory=true)
        │     ├─> FileListModel::setPath(newPath)
        │     ├─> history.append(newPath) + historyIndex++
        │     ├─> emit pathChanged(path) ─> FileTabBar::setTabPath(title, tooltip)
        │     └─> updateActionStates()  // 后退/前进按钮状态
        └─> 若是文件 ─> FileOperations::openWithDefault(item)
```

### 4.3 配置变更广播
```
SettingsDialog::onApply()
  └─> ConfigManager::setValue(section, key, value)   // 同步写盘
        └─> emit configChanged(section)
              ├─> MainWindow::refreshPanelActions() / updateToolbar()
              ├─> PanelWidget（按 section 分发）：
              │     ├─ Panels    → tabBar->setTabsClosable()
              │     ├─ File_Browser → 所有 model->setShowHidden() / setDateTimeFormat()
              │     └─ File_Browser_Columns → ColumnManager::applyToView() 所有 view
              ├─> ColumnManager::applyToAllViews()  (若列配置变更)
              ├─> FmApplication::reloadTranslator() + applyIconTheme()  (若语言/图标主题变更)
              └─> QApplication::setStyle(...)        (若主题变更)
```

### 4.4 单实例路径接收
```
SingleInstance::pathsReceived(paths)
  └─> MainWindow::addPathsToPanels(paths)
        ├─> PanelContainer::activePanel()->addTab(path)
        └─> 若有 path2 ─> PanelContainer::panel(Panel2)->addTab(path2)
```

### 4.5 文件操作进度（预扫描 + 异步执行）
```
FileOperations::copy(sources, dest)
  └─> runCopyMove(sources, dest, isMove=false)
        ├─> [主线程预扫描] 遍历 sources:
        │     ├─> 检查是否为源目录或其子目录（canonicalPath）
        │     ├─> 同文件夹粘贴 → uniqueName() 自动重命名
        │     └─> 目标存在 → resolveConflict() 弹 ConflictDialog（同步）
        │           └─> Cancel → break 整个操作
        ├─> [创建 ProgressDialog]（在冲突解决之后，避免 showDelayed 定时器干扰）
        └─> QtConcurrent::run(copyRecursively, ...)
              ├─> 每块复制后 QMetaObject::invokeMethod(progress, lambda, QueuedConnection) 更新进度
              ├─> 检测 canceled → 关闭文件 + QFile::remove(dst) 删除不完整文件
              └─> 完成后 emit directoryChanged(destDir) → PanelWidget::刷新该目录所有 model
```

---

## 5. 关键设计决策

### 5.1 模型-视图分离
- `FileListModel` 仅负责数据加载，不感知 UI；15 列定义在 `Column` 枚举中
- `FileListSortProxy` 处理排序逻辑，".."行通过 `IsParentRowRole` 检测并固定首行（需检查 `lastSortOrder_` 以处理降序）
- `FileListView` 仅处理交互，列布局由 `ColumnManager` 注入
- 数值/日期列按实际值排序（qint64 / QDateTime），避免按格式化字符串排序

### 5.2 异步文件操作与预扫描冲突解决
- 文件操作通过 `QtConcurrent::run` 在线程池执行；进度通过 `QMetaObject::invokeMethod(target, lambda, Qt::QueuedConnection)` 线程安全地更新 UI
- **冲突解决在主线程同步执行**（预扫描阶段）：在启动 QtConcurrent 之前，遍历所有源项检查冲突并弹 ConflictDialog
- **没有 QWaitCondition 阻塞**：预扫描完成后才创建 ProgressDialog 与启动异步作业
- **取消机制**：`std::atomic<bool> canceled` 标志位；进度对话框取消后停止后续文件操作，已完成的文件不回滚，未完成的文件 `QFile::remove(dst)` 删除
- **同线程警告**：`QMetaObject::invokeMethod` 使用 `Qt::BlockingQueuedConnection` 从同线程调用会导致死锁或静默失败——必须直接调用

### 5.3 选项卡拖拽同步
- `QTabBar::tabMoved(from, to)` 信号触发同步：
  1. `tabs_.move(from, to)` 同步 QList 顺序
  2. `stack_->insertWidget(to, w)` 重新定位 QStackedWidget 中的 widget（已存在的 widget 会被移动而非复制）
  3. `stack_->setCurrentIndex(tabBar_->currentIndex())` 确保当前显示一致
- `FileListModel::pathChanged` 信号 lambda 不捕获固定 index，而是按 model 指针动态查找 `tabs_` 中的当前索引（避免拖拽后索引失效）

### 5.4 全局状态集中管理
- 单例：`ConfigManager`、`ShortcutManager`、`ColumnManager`、`FileOperations`、`ClipboardManager`、`OpenWithManager`、`VolumeManager`
- 单例通过 `instance()` 访问，避免全局变量散乱
- 状态变更通过信号广播，订阅者各自刷新
- `ShortcutManager` 使用 `QPointer<QAction>` 跟踪 action 绑定（弱引用），修改快捷键后通过 `reapplyShortcuts()` 立即生效

### 5.5 配置错误处理策略
- 启动时 `ConfigManager::load()` 检测损坏
- 损坏时弹出 `QMessageBox` 询问是否重建
- 重建流程：`backupCurrentFile()`（添加 `yyyyMMdd_HHmmss_zzz.bak` 后缀）→ 删除原文件 → 写入默认配置
- 运行时保存失败：仅提示，不退出，内存配置保留
- `ensureDefaultConfig()` 写入默认值：`Panels/tabsClosable=false`、`File_Operations/chunkSizeMB=1`、`File_Browser/dateTimeFormat="yyyy-MM-dd HH:mm"` 等

### 5.6 i18n 翻译流水线
- 所有 UI 文本通过 `tr()` 包装
- 翻译流水线：`lupdate`（提取源文本到 .ts）→ `translate.py`（Python 脚本匹配英→中映射写入 .ts）→ `lrelease`（编译 .ts 为 .qm 嵌入资源）
- 切换语言时：
  1. `QCoreApplication::removeTranslator(&oldTranslator)`
  2. `translator.load("fm_" + lang)`
  3. `QCoreApplication::installTranslator(&translator)`
  4. 对所有顶层 widget 调用 `ui->retranslateUi()` 或自定义 `retranslateUi()` 方法
- MainWindow 实现 `retranslateUi()` 重建菜单/工具栏文字
- 已是中文的字符串使用 `QStringLiteral()` 而非 `tr()`，避免产生不必要的翻译条目

### 5.7 单实例通信
- 使用 `QLocalServer` 监听 `/tmp/fm-<username>.sock`
- 新启动时尝试 `QLocalSocket::connectToServer`：
  - 连接成功：发送 path 列表（JSON 序列化）后退出
  - 连接失败：创建 server，正常启动

### 5.8 UDisks2 集成
- 通过 `QDBusInterface` 调用 `org.freedesktop.UDisks2`
- 枚举外部设备：`Manager.GetBlockDevices(QVariantMap{})` → 返回 `ao` 对象路径列表
- 挂载：`Filesystem.Mount(QVariantMap)` → 返回挂载点字符串
- 卸载：`Filesystem.Unmount(QVariantMap)`
- 弹出：`Drive.Eject(QVariantMap)`
- 设备文件 `/dev/sdXN` 通过 `toUDisks2ObjectPath()` 转换为 D-Bus 对象路径
- `aay` 类型属性（如 `MountPoints`）需通过 `Properties.Get` + `QDBusArgument >>` 手动反序列化

### 5.9 图标主题
- 通过 `QIcon::fromTheme` 获取系统图标主题
- 图标主题可通过设置对话框配置（保存到 `[UI] iconTheme`）
- 程序启动时 `FmApplication::applyIconTheme()` 读取并应用，修改后实时生效
- 默认"自动"：优先使用 `gnome-icon-theme`（包含完整的标准动作图标 .png 文件，避免 Adwaita 等仅提供 *-symbolic.svg 的主题导致 QAction 图标无法显示），不可用时回退系统默认（hicolor）
- 使用的图标名必须存在于 gnome-icon-theme 中

### 5.10 文件属性对话框扩展属性
- ACL 通过 `QProcess` 调用 `getfacl -cp` 获取（-c 去除注释，-p 不省略路径）
- ext2/ext3/ext4/btrfs/xfs 标志位通过 `QProcess` 调用 `lsattr -d` 获取（-d 仅显示目录自身）
- QProcess 超时 3 秒；命令不存在或失败时回退为"(不可用)"
- `stat()` 系统调用填充 ownerId（`st_uid`）、groupId（`st_gid`）、diskUsage（`st_blocks * 512`）、accessed（`st_atime`）、statusChanged（`st_ctime`）

---

## 6. 依赖关系

### 6.1 Qt 模块依赖
- `Qt6::Widgets`、`Qt6::Core`、`Qt6::Gui`
- `Qt6::Concurrent`（异步文件操作）
- `Qt6::DBus`（UDisks2）
- `Qt6::Network`（QLocalServer/QLocalSocket 单实例通信）
- `Qt6::LinguistTools`（i18n）

### 6.2 模块间依赖方向
```
app → ui → panel → filelist → core
              │        │
              ▼        ▼
           fileops   (volume 已并入 core)
              │        │
              └──→ dialogs
```
- `core` 是最底层，不依赖其他业务模块（含 config_manager / shortcut_manager / column_manager / clipboard_manager / open_with_manager / volume_manager / session_state / favorite_manager）
- `dialogs` 依赖 `core` 与 `fileops`（含 settings_dialog / settings_pages / properties_dialog / open_with_dialog / conflict_dialog 等）
- 没有独立的 `volume/` 与 `settings/` 目录——`volume_manager` 在 `core/`，`settings_dialog` 与 `settings_pages` 在 `dialogs/`
- 避免循环依赖

### 6.3 外部依赖
- Linux udev / UDisks2 D-Bus 服务（系统自带）
- `xdg-mime`、`xdg-open` 命令（xdg-utils 包）
- `getfacl`（可选，属性对话框 ACL 显示）
- `lsattr`（可选，属性对话框 ext 标志位显示）
- 系统图标主题（推荐 `gnome-icon-theme`）
- Python 3（构建时运行 `translate.py`，非运行时依赖）

---

## 7. CMake 结构概要

```cmake
cmake_minimum_required(VERSION 3.21)
project(fm VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Widgets Core Gui Concurrent DBus Network LinguistTools)

qt_standard_project_setup()

qt_add_executable(fm
    src/main.cpp
    src/app/fm_application.h src/app/fm_application.cpp
    src/app/single_instance.h src/app/single_instance.cpp
    src/core/clipboard_manager.h src/core/clipboard_manager.cpp
    src/core/column_manager.h src/core/column_manager.cpp
    src/core/config_manager.h src/core/config_manager.cpp
    src/core/favorite_manager.h src/core/favorite_manager.cpp
    src/core/open_with_manager.h src/core/open_with_manager.cpp
    src/core/session_state.h src/core/session_state.cpp
    src/core/shortcut_manager.h src/core/shortcut_manager.cpp
    src/core/volume_manager.h src/core/volume_manager.cpp
    src/dialogs/*.h src/dialogs/*.cpp   # 含 settings_dialog / settings_pages
    src/filelist/*.h src/filelist/*.cpp
    src/fileops/*.h src/fileops/*.cpp
    src/panel/*.h src/panel/*.cpp
    src/ui/main_window.h src/ui/main_window.cpp
    resources.qrc
)

target_link_libraries(fm PRIVATE
    Qt6::Widgets Qt6::Core Qt6::Gui
    Qt6::Concurrent Qt6::DBus Qt6::Network
)

# 翻译文件（.ts → .qm → 嵌入资源 /i18n）
qt_add_translations(fm
    TS_FILES translations/fm_zh.ts translations/fm_en.ts
    RESOURCE_PREFIX "/i18n"
)

install(TARGETS fm RUNTIME DESTINATION bin)
install(FILES fm.1 DESTINATION share/man/man1)                              # man 手册
install(FILES fm.desktop DESTINATION share/applications)                    # .desktop
install(FILES fm.png DESTINATION share/icons/hicolor/512x512/apps RENAME fm.png)  # 图标
```
- 自定义构建目标 `fm_lupdate` 调用 `lupdate` 提取源文本到 .ts 文件
- `translate.py` 在 `translations/` 目录手动运行，将英→中映射写入 `fm_zh.ts`
- `lrelease`（由 `qt_add_translations` 自动调用）编译 .ts 为 .qm 并嵌入 `resources.qrc` 的 `/i18n` 前缀

---

## 8. 实现里程碑（历史）

> 以下为初始规划的阶段划分，**所有阶段均已实现完成**。保留作为开发历史参考。

### Phase 1：基础骨架（已完成）
1. CMake 工程 + main + FmApplication
2. MainWindow + MenuBar（含文件/收藏/设置/帮助菜单）
3. PanelContainer + PanelWidget + FileTabBar（多选项卡 + 拖拽同步）
4. FileListModel（15 列）+ FileListView + FileListSortProxy（数值/日期排序）
5. ConfigManager + SessionState（含损坏处理与备份）

### Phase 2：核心文件操作（已完成）
6. FileOperations（复制/剪切/粘贴/删除 + 分块复制 + 取消删除不完整文件）
7. ClipboardManager（URI 形式 + 剪切标记）
8. 右键菜单（无选中 + 有选中 + ".." 行无选中）
9. 导航历史（每选项卡独立 history + historyIndex）
10. 列宽/列顺序调整（像素单位 + Stretch 模式）

### Phase 3：配置与设置（已完成）
11. 完整 ConfigManager（损坏处理 + 默认配置 + 备份）
12. 设置对话框（5 个分组页：界面/面板/文件浏览/文件操作/快捷键）
13. ShortcutManager（QPointer 跟踪 + reapplyShortcuts + 冲突检测）
14. FavoriteManager（percent-encoding + 右键删除）
15. ColumnManager（registerView/unregisterView + 信号监听）

### Phase 4：高级功能（已完成）
16. VolumeManager（QStorageInfo + UDisks2 + 外部设备枚举）
17. TrashCan（FreeDesktop 规范 + .Trash/.Trash-<uid>）
18. ProgressDialog + ConflictDialog（预扫描 + atomic 取消）
19. 单实例（QLocalServer + JSON 路径传递）
20. OpenWithDialog + OpenWithManager（"记住此选择" + xdg-mime）

### Phase 5：完善（已完成）
21. i18n（中英文翻译流水线：lupdate → translate.py → lrelease）
22. PropertiesDialog（3 分组 + ACL + ext 标志位 + stat 系统调用）
23. AboutDialog
24. 键盘导航 + Tab 焦点（F5/F6 复制/剪切到对面 + Ctrl+Tab 切换选项卡）
25. 主题切换 + 图标主题（gnome-icon-theme 默认）
26. 防止复制/移动文件夹到自身或其子目录（canonicalPath 检查）
27. 选项卡关闭按钮可配置（默认不启用）
28. 日期时间格式可配置（默认 yyyy-MM-dd HH:mm）
29. 粘贴分块大小可配置（默认 1 MB）
30. man 手册 + .desktop + 图标安装（CMake 集成）
