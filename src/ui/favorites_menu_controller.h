#ifndef FM_UI_FAVORITES_MENU_CONTROLLER_H
#define FM_UI_FAVORITES_MENU_CONTROLLER_H

#include <QObject>

class QMenu;
class QAction;

namespace fm {

// 收藏菜单控制器
// - 构建"添加到收藏..."菜单项 + 分隔符 + 动态收藏列表
// - aboutToShow 时从 FavoriteManager 刷新收藏列表
// - 右键收藏项弹出删除菜单
// - 左键点击收藏项通过 favoriteTriggered 信号通知宿主恢复布局
// - 点击"添加到收藏..."通过 addFavoriteRequested 信号通知宿主采集当前布局并保存
class FavoritesMenuController : public QObject
{
    Q_OBJECT
public:
    explicit FavoritesMenuController(QMenu *favoritesMenu, QObject *parent = nullptr);

    // 创建"添加到收藏..."项、分隔符、占位项，连接 aboutToShow，安装 eventFilter。
    void setup();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void refresh();

signals:
    // 用户点击"添加到收藏..."，宿主负责弹出输入框并保存当前布局
    void addFavoriteRequested();
    // 用户点击收藏项，宿主负责加载布局并恢复
    void favoriteTriggered(const QString &name);

private:
    QMenu *favoritesMenu_;
};

} // namespace fm

#endif // FM_UI_FAVORITES_MENU_CONTROLLER_H
