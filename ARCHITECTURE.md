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
│   main / FmApplication / SingleInstance / CommandLineParser  │
└───────────────────────────┬─────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ▼                   ▼                   ▼
┌─────────────────┐  ┌──────────────┐  ┌─────────────────┐
│   ui (界面)     │  │ core (核心)  │  │  fileops (操作) │
│ MainWindow      │  │ ConfigManager│  │ FileOperations  │
│ MenuBarManager  │  │ SessionState │  │ CopyMoveJob     │
│ ToolBarManager  │  │ FavoriteMgr  │  │ DeleteJob       │
└────────┬────────┘  │ ShortcutMgr  │  │ TrashJob        │
         │           │ ClipboardMgr │  │ ProgressDialog  │
         │           └──────┬───────┘  └────────┬────────┘
         │                  │                   │
         ▼                  ▼                   ▼
┌─────────────────────────────────────────────────────────┐
│                  panel / filelist                        │
│  PanelContainer / PanelWidget / FileTabBar              │
│  FileListView / FileListModel / FileListSortProxy       │
│  FileItem / FileListDelegate / ColumnManager            │
└─────────────────────────┬───────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ volume       │  │ settings     │  │ dialogs      │
│ VolumeMgr    │  │ SettingsDlg  │  │ PropertiesDlg│
│ VolumeMenu   │  │ ISettingsPage│  │ OpenWithDlg  │
│ (UDisks2)    │  │ 4 pages      │  │ AboutDlg ... │
└──────────────┘  └──────────────┘  └──────────────┘
```

---

## 2. 目录结构

```
fm-qt/
├── CMakeLists.txt
├── REQUIREMENTS.md
├── ARCHITECTURE.md
├── src/
│   ├── main.cpp                      # 程序入口
│   ├── app/
│   │   ├── fm_application.h           # QApplication 子类
│   │   ├── fm_application.cpp
│   │   ├── single_instance.h          # 单实例管理（QLocalServer）
│   │   ├── single_instance.cpp
│   │   ├── command_line_parser.h      # 命令行参数解析
│   │   └── command_line_parser.cpp
│   ├── core/
│   │   ├── config_manager.h           # 配置文件管理（QSettings 封装）
│   │   ├── config_manager.cpp
│   │   ├── session_state.h            # 会话状态（布局/选项卡/排序）
│   │   ├── session_state.cpp
│   │   ├── favorite_manager.h         # 收藏项管理
│   │   ├── favorite_manager.cpp
│   │   ├── shortcut_manager.h         # 快捷键管理
│   │   ├── shortcut_manager.cpp
│   │   ├── clipboard_manager.h        # 剪贴板管理（URI/剪切标记）
│   │   └── clipboard_manager.cpp
│   ├── ui/
│   │   ├── main_window.h
│   │   ├── main_window.cpp
│   │   ├── menu_bar_manager.h         # 菜单栏构建与刷新
│   │   ├── menu_bar_manager.cpp
│   │   ├── tool_bar_manager.h         # 工具栏构建与刷新
│   │   └── tool_bar_manager.cpp
│   ├── panel/
│   │   ├── panel_container.h         # 双面板容器（布局/分隔条/显隐）
│   │   ├── panel_container.cpp
│   │   ├── panel_widget.h            # 单个面板（选项卡栏 + 文件列表）
│   │   ├── panel_widget.cpp
│   │   ├── file_tab_bar.h            # 选项卡栏（支持拖拽）
│   │   ├── file_tab_bar.cpp
│   │   └── panel_id.h                # PanelId 枚举（Panel1/Panel2）
│   ├── filelist/
│   │   ├── file_list_view.h          # QTreeView 子类
│   │   ├── file_list_view.cpp
│   │   ├── file_list_model.h         # QAbstractItemModel
│   │   ├── file_list_model.cpp
│   │   ├── file_list_sort_proxy.h    # QSortFilterProxyModel
│   │   ├── file_list_sort_proxy.cpp
│   │   ├── file_item.h               # 文件项数据结构
│   │   ├── file_item.cpp
│   │   ├── file_list_delegate.h      # 重命名编辑等委托
│   │   ├── file_list_delegate.cpp
│   │   ├── column_manager.h          # 列可见性/顺序/列宽（像素）管理
│   │   ├── column_manager.cpp
│   │   └── column_def.h               # 列定义枚举
│   ├── fileops/
│   │   ├── file_operations.h         # 文件操作门面
│   │   ├── file_operations.cpp
│   │   ├── copy_move_job.h           # 异步复制/移动作业
│   │   ├── copy_move_job.cpp
│   │   ├── delete_job.h              # 异步彻底删除作业
│   │   ├── delete_job.cpp
│   │   ├── trash_job.h               # 移到回收站作业
│   │   ├── trash_job.cpp
│   │   ├── progress_dialog.h         # 进度对话框
│   │   ├── progress_dialog.cpp
│   │   ├── conflict_resolver.h       # 同名冲突处理
│   │   ├── conflict_resolver.cpp
│   │   ├── trash_can.h               # FreeDesktop Trash 规范实现
│   │   └── trash_can.cpp
│   ├── volume/
│   │   ├── volume_manager.h          # UDisks2 D-Bus 封装
│   │   ├── volume_manager.cpp
│   │   ├── volume_menu.h             # 文件菜单中的卷列表
│   │   ├── volume_menu.cpp
│   │   ├── udisks2_client.h          # D-Bus 接口定义
│   │   └── udisks2_client.cpp
│   ├── settings/
│   │   ├── settings_dialog.h          # 设置对话框主框架
│   │   ├── settings_dialog.cpp
│   │   ├── settings_page_interface.h # 设置页面接口
│   │   ├── ui_settings_page.h         # 界面分组页
│   │   ├── ui_settings_page.cpp
│   │   ├── panel_settings_page.h      # 面板分组页
│   │   ├── panel_settings_page.cpp
│   │   ├── file_browser_settings_page.h # 文件浏览分组页
│   │   ├── file_browser_settings_page.cpp
│   │   ├── shortcut_settings_page.h   # 快捷键分组页
│   │   └── shortcut_settings_page.cpp
│   └── dialogs/
│       ├── about_dialog.h
│       ├── about_dialog.cpp
│       ├── properties_dialog.h
│       ├── properties_dialog.cpp
│       ├── open_with_dialog.h        # "打开..."选择应用
│       ├── open_with_dialog.cpp
│       ├── input_name_dialog.h       # 收藏新建/重名提示
│       ├── input_name_dialog.cpp
│       ├── conflict_dialog.h         # 同名冲突选择对话框
│       ├── conflict_dialog.cpp
│       ├── error_dialog.h             # 错误提示对话框
│       └── error_dialog.cpp
├── translations/
│   ├── fm_en.ts                      # 英文翻译（默认）
│   └── fm_zh_CN.ts                   # 中文翻译
├── resources/
│   ├── fm.qrc
│   └── icons/                         # 兜底图标（系统主题缺失时）
└── tests/                             # 单元测试
    ├── test_config_manager.cpp
    ├── test_file_item.cpp
    └── test_trash_can.cpp
```

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

    // 通用读写
    QVariant value(const QString &section, const QString &key) const;
    void setValue(const QString &section, const QString &key, const QVariant &value);

    // 错误处理
    bool isCorrupted() const;
    bool rebuildDefault();      // 备份原文件 + 重建默认配置
    QString backupFilePath() const;

signals:
    void configChanged(const QString &section);  // 配置变更通知

private:
    QSettings *settings_;
    bool corrupted_;
};
```

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
    int activePanelIndex;
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
**职责**：快捷键映射、冲突检测、应用到 QAction
```cpp
struct ShortcutItem {
    QString id;            // 如 "file.refresh"
    QString defaultKey;    // 默认快捷键
    QString currentKey;    // 用户配置
    bool conflicted;       // 是否冲突
};

class ShortcutManager : public QObject {
    Q_OBJECT
public:
    static ShortcutManager *instance();

    QList<ShortcutItem> allShortcuts() const;
    QString shortcutFor(const QString &id) const;
    bool setShortcut(const QString &id, const QKeySequence &seq);
    void applyToAction(QAction *action, const QString &id);  // 绑定到 QAction

    void detectConflicts();    // 启动时检测，仅第一个生效

signals:
    void shortcutsChanged();

private:
    QMap<QString, ShortcutItem> items_;   // id -> item
};
```

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

---

### 3.3 主窗口层

#### `MainWindow`（QMainWindow）
**职责**：组装菜单栏、工具栏、双面板容器；处理关闭事件保存布局
```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void addPathsToPanels(const QStringList &paths);   // 单实例接收到 path

protected:
    void closeEvent(QCloseEvent *event) override;       // 保存 Session 后退出

private slots:
    void onSettingsChanged(const QString &section);

private:
    MenuBarManager *menuBarManager_;
    ToolBarManager *toolBarManager_;
    PanelContainer *panelContainer_;
};
```

#### `MenuBarManager`
**职责**：构建文件/收藏/设置/帮助菜单，动态刷新卷菜单与收藏菜单，绑定快捷键
```cpp
class MenuBarManager : public QObject {
    Q_OBJECT
public:
    MenuBarManager(MainWindow *window);
    void rebuildVolumeMenu();      // 打开时实时枚举
    void rebuildFavoritesMenu();
    void refreshActionStates();     // 切换活动面板等状态后刷新文字/选中

signals:
    void volumeClicked(const QString &mountPoint);
    void volumeUnmountRequested(const QString &device);
    void volumeEjectRequested(const QString &device);
    void favoriteNewRequested();
    void favoriteActivated(const QString &name);
    void favoriteDeleteRequested(const QString &name);
    // ... 各种设置菜单触发信号

private:
    QMenu *fileMenu_, *favoritesMenu_, *settingsMenu_, *helpMenu_;
    VolumeMenu *volumeMenu_;
};
```

#### `ToolBarManager`
**职责**：根据当前活动选项卡的选中状态动态构建工具栏按钮
```cpp
class ToolBarManager : public QObject {
    Q_OBJECT
public:
    ToolBarManager(QToolBar *toolbar, PanelContainer *container);

public slots:
    void onActiveTabSelectionChanged(const QItemSelection &selected);
    void onActiveTabChanged();

private:
    void rebuildActions(bool hasSelection, bool singleSelection, bool singleIsFolder);
    QToolBar *toolbar_;
    QList<QAction*> actions_;
};
```

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
**职责**：单个面板，包含选项卡栏 + 当前文件列表视图
```cpp
class PanelWidget : public QWidget {
    Q_OBJECT
public:
    PanelWidget(PanelId id, QWidget *parent = nullptr);

    PanelId id() const { return id_; }

    // 选项卡管理
    int addTab(const QString &path, int index = -1);     // -1=追加
    void closeTab(int index);
    void closeOtherTabs(int index);
    int cloneTab(int index);                              // 返回新选项卡索引
    int tabCount() const;
    int activeTabIndex() const;
    void setActiveTab(int index);
    QString tabPath(int index) const;
    QString activeTabPath() const;

    // 文件列表
    FileListView *listView() const;
    FileListModel *model() const;
    QList<FileItem> selectedItems() const;

signals:
    void activeTabChanged(int index);
    void tabCountChanged();
    void pathChanged(const QString &path);
    void selectionChanged(const QItemSelection &selected);
    void folderOpenRequested(const QString &path);  // 进入子文件夹（记入历史）
    void parentDirRequested();                       // 点击 ".."

public slots:
    void navigateBack();
    void navigateForward();
    void navigateUp();

private:
    PanelId id_;
    FileTabBar *tabBar_;
    QStackedWidget *stack_;        // 每个选项卡一个 FileListView
    QList<FileListView*> views_;
    NavigationHistory history_;   // 该选项卡独立，实际存于每个 view
};
```

#### `FileTabBar`（QTabBar 子类）
**职责**：选项卡标题、tooltip、拖拽重排、"+"/"×" 按钮
```cpp
class FileTabBar : public QTabBar {
    Q_OBJECT
public:
    FileTabBar(QWidget *parent = nullptr);

    void setTabPath(int index, const QString &path);   // 自动设置标题（截断16字符）+ tooltip

signals:
    void newTabRequested();
    void closeTabRequested(int index);
    void tabMoved(int from, int to);
    void contextMenuRequested(int index, const QPoint &pos);

protected:
    void mousePressEvent(QMouseEvent *event) override;    // 检测 "+" 按钮
    void contextMenuEvent(QContextMenuEvent *event) override;
    void startDrag(int index);
};
```

---

### 3.5 文件列表层

#### `FileItem`（数据结构）
```cpp
struct FileItem {
    QString name;              // 文件全名
    QString absolutePath;
    qint64 size = 0;
    bool isDir = false;
    bool isSymLink = false;
    QString symLinkTarget;
    QString mimeTypeName;     // image/png
    QString mimeTypeComment;  // PNG 图像
    QIcon icon;
    QString owner;
    QString group;
    QDateTime created;
    QDateTime modified;
    QFile::Permissions permissions;
    quint64 inode = 0;
};
```

#### `FileListModel`（QAbstractItemModel）
**职责**：加载目录、提供行数据、管理 ".." 行
```cpp
class FileListModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column {
        ColIcon, ColName, ColSize, ColType, ColMimeType,
        ColGroup, ColOwner, ColCreated, ColModified, ColPermissions
    };

    void setPath(const QString &path);
    QString path() const;

    bool showHidden() const;
    void setShowHidden(bool show);

    // 行数据
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    FileItem itemAt(const QModelIndex &index) const;
    bool isParentRow(const QModelIndex &index) const;   // ".." 行

signals:
    void pathChanged(const QString &path);
    void loadError(const QString &errorMsg);    // 无权限/不存在

private:
    QString path_;
    QList<FileItem> items_;
    bool showHidden_;
    bool hasParent_;     // 是否显示 ".."
};
```

#### `FileListSortProxy`（QSortFilterProxyModel）
**职责**：处理列头排序、次要排序（保留上次排序条件）、".."行不参与排序固定第一行
```cpp
class FileListSortProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    void setSortColumn(int column, Qt::SortOrder order);
    QModelIndex parentRowIndex() const;    // 返回 ".." 行索引

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    // ".." 行始终排第一，且不参与 lessThan
};
```

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

#### `ColumnManager`
**职责**：列可见性、列顺序、列宽（像素）的全局管理与持久化
```cpp
struct ColumnConfig {
    QList<FileListModel::Column> visibleColumns;  // 按顺序
    QMap<FileListModel::Column, int> columnWidths;  // 像素宽度（Name 列除外，Stretch 自动填充）
};

class ColumnManager : public QObject {
    Q_OBJECT
public:
    static ColumnManager *instance();
    ColumnConfig config() const;
    void setConfig(const ColumnConfig &config);
    void applyToFileListView(FileListView *view);

signals:
    void columnsChanged();
};
```

---

### 3.6 文件操作层

#### `FileOperations`（门面）
**职责**：统一入口，创建 Job 并管理进度对话框
```cpp
class FileOperations : public QObject {
    Q_OBJECT
public:
    static FileOperations *instance();

    void copy(const QList<QUrl> &sources, const QString &destDir);
    void move(const QList<QUrl> &sources, const QString &destDir);
    void trash(const QList<QUrl> &sources);
    void deletePermanently(const QList<QUrl> &sources);
    void pasteFromClipboard(const QString &destDir);
    void rename(const QUrl &target, const QString &newName);
    void createFile(const QString &dir, const QString &name);
    void createDir(const QString &dir, const QString &name);

signals:
    void operationCompleted();
    void operationFailed(const QString &errorMsg);

private:
    void runJob(AbstractJob *job);
    ProgressDialog *progressDialog_;
};
```

#### `AbstractJob`（接口）
```cpp
class AbstractJob : public QObject {
    Q_OBJECT
public:
    virtual void start() = 0;
    void cancel();

signals:
    void progress(int percent, const QString &currentFile);
    void conflictEncountered(const QUrl &source, const QString &destPath,
                            ConflictResolution *resolution);
    void finished(bool success, const QString &errorMsg);

protected:
    volatile bool cancelled_;
};
```

#### `CopyMoveJob`、`DeleteJob`、`TrashJob`：具体作业
使用 `QtConcurrent::run` 在线程池执行，通过信号回报进度。

#### `ConflictResolver`
**职责**：弹出冲突对话框，返回用户选择，支持"全部"模式
```cpp
enum class ConflictResolution {
    Overwrite, Skip, Rename,
    OverwriteAll, SkipAll, RenameAll,
    Cancel
};

class ConflictResolver : public QObject {
    Q_OBJECT
public:
    ConflictResolution resolve(const QUrl &source, const QString &dest);
    void resetBatchMode();   // 重置"全部"记忆
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

---

### 3.7 卷管理层

#### `VolumeManager`
**职责**：通过 UDisks2 D-Bus 枚举卷、挂载、卸载、弹出
```cpp
struct VolumeInfo {
    QString devicePath;       // /dev/sdb1
    QString mountPoint;       // /media/user/USB（空表示未挂载）
    QString label;
    bool isMounted;
    bool isRemovable;
    QIcon icon;
};

class VolumeManager : public QObject {
    Q_OBJECT
public:
    QList<VolumeInfo> listVolumes();   // 打开菜单时调用

    bool mount(const QString &device, QString *errorMsg);
    bool unmount(const QString &device, QString *errorMsg);
    bool eject(const QString &device, QString *errorMsg);

private:
    OrgFreedesktopUDisks2FilesystemInterface *iface_;
};
```

#### `VolumeMenu`
**职责**：构建文件菜单中的卷子菜单，左键打开，右键弹出卸载/弹出
```cpp
class VolumeMenu : public QObject {
    Q_OBJECT
public:
    VolumeMenu(QMenu *parent);
    void refresh();   // 菜单 aboutToShow 时调用

signals:
    void volumeOpenRequested(const QString &mountPoint);
    void volumeUnmountRequested(const QString &device);
    void volumeEjectRequested(const QString &device);
};
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
    void showPage(const QString &pageId);   // "ui"/"panel"/"filebrowser"/"shortcuts"
                                              // "设置列"菜单项调用 showPage("filebrowser")

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
    virtual void reset() = 0;       // 恢复到上次 apply 的状态
};
```

#### 四个设置页：`UiSettingsPage`、`PanelSettingsPage`、`FileBrowserSettingsPage`、`ShortcutSettingsPage`

`ShortcutSettingsPage` 使用 `QTableWidget` + `QKeySequenceEdit` 委托，冲突项用 `QTableWidgetItem::setForeground` 设为红色。

---

### 3.9 对话框层

#### `OpenWithDialog`
**职责**：通过 `xdg-mime query default <mime>` 与解析 `/usr/share/applications/*.desktop` 列出可用程序
```cpp
class OpenWithDialog : public QDialog {
public:
    OpenWithDialog(const QString &mimeType, QWidget *parent);
    QString selectedDesktopFile() const;
    bool rememberChoice() const;
};
```

#### `PropertiesDialog`
**职责**：分组展示属性，符号链接仅显示直接目标

#### `ConflictDialog`
**职责**：6 个按钮（覆盖/跳过/重命名/全部覆盖/全部跳过/全部重命名）+ 取消

---

## 4. 信号槽关系图

### 4.1 启动流程
```
main()
  └─> FmApplication::initialize()
        ├─> SingleInstance::tryLock()  ─失败─> sendPaths() ─> 退出
        ├─> ConfigManager::instance()->load()  ─损坏─> 提示重建/退出
        ├─> ShortcutManager::detectConflicts()
        ├─> QTranslator::load(defaultLang)
        └─> MainWindow::show()
```

### 4.2 文件列表导航
```
FileListView::openRequested(index)
  ├─> 若是文件夹 ─> PanelWidget::folderOpenRequested(path)
  │     ├─> FileListModel::setPath(newPath)
  │     ├─> NavigationHistory::push(newPath)
  │     └─> FileTabBar::setTabPath(title, tooltip)
  └─> 若是文件 ─> FileOperations::openWithDefault(item)
```

### 4.3 配置变更广播
```
SettingsDialog::onApply()
  └─> ConfigManager::setValue(...)
        └─> emit configChanged(section)
              ├─> MenuBarManager::refreshActionStates()
              ├─> ColumnManager::applyToAllViews()
              ├─> 所有 FileListModel::setShowHidden()
              ├─> FmApplication::reloadTranslator()  (若语言变更)
              └─> QApplication::setStyle(...)        (若主题变更)
```

### 4.4 单实例路径接收
```
SingleInstance::pathsReceived(paths)
  └─> MainWindow::addPathsToPanels(paths)
        ├─> PanelContainer::activePanel()->addTab(path)
        └─> 若有 path2 ─> PanelContainer::panel(Panel2)->addTab(path2)
```

### 4.5 文件操作进度
```
FileOperations::copy(sources, dest)
  └─> CopyMoveJob::start()  (QtConcurrent)
        ├─> progress() ─> ProgressDialog::update()
        ├─> conflictEncountered() ─> ConflictResolver::resolve() ─> 阻塞等待
        └─> finished() ─> ProgressDialog::close() + 目标 FileListModel::refresh()
```

---

## 5. 关键设计决策

### 5.1 模型-视图分离
- `FileListModel` 仅负责数据加载，不感知 UI
- `FileListSortProxy` 处理排序逻辑，".."行通过 `filterAcceptsRow` + `lessThan` 强制排第一
- `FileListView` 仅处理交互，列布局由 `ColumnManager` 注入

### 5.2 异步作业
- 所有文件操作通过 `QtConcurrent::run` 在线程池执行
- 主线程通过信号接收进度/冲突/完成通知
- 冲突解决通过 `QWaitCondition` 阻塞工作线程，主线程弹对话框后唤醒

### 5.3 全局状态集中管理
- 单例：`ConfigManager`、`ShortcutManager`、`ColumnManager`、`FileOperations`、`ClipboardManager`
- 单例通过 `instance()` 访问，避免全局变量散乱
- 状态变更通过信号广播，订阅者各自刷新

### 5.4 配置错误处理策略
- 启动时 `ConfigManager::load()` 检测损坏
- 损坏时弹出 `QMessageBox` 询问是否重建
- 重建流程：`QFile::rename` 加时间戳后缀 → 写入默认配置 → 重新加载
- 运行时保存失败：仅提示，不退出，内存配置保留

### 5.5 i18n 切换
- 所有 UI 文本通过 `tr()` 包装
- 切换语言时：
  1. `QCoreApplication::removeTranslator(&oldTranslator)`
  2. `translator.load("fm_" + lang)`
  3. `QCoreApplication::installTranslator(&translator)`
  4. 对所有顶层 widget 调用 `ui->retranslateUi()` 或自定义 `retranslateUi()` 方法
- MainWindow 实现 `retranslateUi()` 重建菜单/工具栏文字

### 5.6 单实例通信
- 使用 `QLocalServer` 监听 `/tmp/fm-<username>.sock`
- 新启动时尝试 `QLocalSocket::connectToServer`：
  - 连接成功：发送 path 列表（JSON 序列化）后退出
  - 连接失败：创建 server，正常启动

### 5.7 UDisks2 集成
- 通过 `QDBusInterface` 调用 `org.freedesktop.UDisks2`
- 枚举：遍历 `/org/freedesktop/UDisks2/block_devices`
- 挂载：调用 `Filesystem.Mount()` 方法
- 卸载：调用 `Filesystem.Unmount()`
- 弹出：调用 `Drive.Eject()`

---

## 6. 依赖关系

### 6.1 Qt 模块依赖
- `Qt6::Widgets`、`Qt6::Core`、`Qt6::Gui`
- `Qt6::Concurrent`（异步文件操作）
- `Qt6::DBus`（UDisks2）
- `Qt6::LinguistTools`（i18n）

### 6.2 模块间依赖方向
```
app → ui → panel → filelist → core
              │        │
              ▼        ▼
           fileops   volume
              │        │
              └──→ dialogs ← settings
```
- `core` 是最底层，不依赖其他业务模块
- `dialogs` 依赖 `core` 与 `fileops`
- 避免循环依赖

### 6.3 外部依赖
- Linux udev / UDisks2 D-Bus 服务（系统自带）
- `xdg-mime`、`xdg-open` 命令（xdg-utils 包）
- 系统图标主题（用户自行安装）

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

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Core Gui Concurrent DBus LinguistTools)

qt_standard_project_setup()

qt_add_executable(fm
    src/main.cpp
    # app/...
    # core/...
    # ui/...
    # panel/...
    # filelist/...
    # fileops/...
    # volume/...
    # settings/...
    # dialogs/...
)

target_link_libraries(fm PRIVATE
    Qt6::Widgets Qt6::Core Qt6::Gui
    Qt6::Concurrent Qt6::DBus
)

qt_add_translations(fm TS_FILES
    translations/fm_en.ts
    translations/fm_zh_CN.ts
)

install(TARGETS fm RUNTIME DESTINATION bin)
install(FILES resources/fm.qrc DESTINATION share/fm)
```

---

## 8. 实现优先级建议

### Phase 1：基础骨架（可运行）
1. CMake 工程 + main + FmApplication
2. MainWindow + MenuBar（仅"退出"）
3. PanelContainer + PanelWidget + FileTabBar（单选项卡）
4. FileListModel + FileListView + FileListSortProxy
5. ConfigManager + SessionState（最小化）

### Phase 2：核心文件操作
6. FileOperations（复制/剪切/粘贴/删除）
7. ClipboardManager
8. 右键菜单（无选中 + 有选中）
9. 导航历史（后退/前进/上一级）
10. 列宽/列顺序调整

### Phase 3：配置与设置
11. 完整 ConfigManager（损坏处理）
12. 设置对话框（4 个分组页）
13. ShortcutManager + 冲突检测
14. FavoriteManager
15. ColumnManager

### Phase 4：高级功能
16. VolumeManager（UDisks2）+ VolumeMenu
17. TrashCan（FreeDesktop 规范）
18. ProgressDialog + ConflictResolver
19. 单实例（QLocalServer）
20. OpenWithDialog + "记住此选择"

### Phase 5：完善
21. i18n（中英文翻译）
22. PropertiesDialog
23. AboutDialog
24. 键盘导航 + Tab 焦点
25. 主题切换
