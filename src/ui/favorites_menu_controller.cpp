#include "favorites_menu_controller.h"

#include "../core/favorite_manager.h"

#include <QAction>
#include <QMenu>
#include <QMouseEvent>

namespace fm {

FavoritesMenuController::FavoritesMenuController(QMenu *favoritesMenu, QObject *parent)
    : QObject(parent), favoritesMenu_(favoritesMenu) {
}

void FavoritesMenuController::setup() {
    auto *addAction = favoritesMenu_->addAction(tr("&Add to Favorites..."),
        this, &FavoritesMenuController::addFavoriteRequested);
    addAction->setIcon(QIcon::fromTheme(QStringLiteral("bookmark-new")));

    favoritesMenu_->addSeparator();

    // 占位项，refresh 会替换
    auto *placeholder = favoritesMenu_->addAction(tr("(No favorites)"));
    placeholder->setEnabled(false);

    // 菜单显示前刷新
    connect(favoritesMenu_, &QMenu::aboutToShow, this, &FavoritesMenuController::refresh);
    // 安装事件过滤器，支持右键删除收藏项
    favoritesMenu_->installEventFilter(this);
}

void FavoritesMenuController::refresh() {
    if (!favoritesMenu_) return;
    // 清空菜单（保留前两项 + 分隔符）
    const auto actions = favoritesMenu_->actions();
    // 找到分隔符位置
    int sepIndex = -1;
    for (int i = 0; i < actions.size(); ++i) {
        if (actions.at(i)->isSeparator()) {
            sepIndex = i;
            break;
        }
    }
    // 删除分隔符之后的所有项
    if (sepIndex >= 0) {
        for (int i = actions.size() - 1; i > sepIndex; --i) {
            favoritesMenu_->removeAction(actions.at(i));
        }
    }

    // 使用 FavoriteManager 获取收藏列表
    const QStringList names = FavoriteManager::instance()->favoriteNames();
    if (names.isEmpty()) {
        auto *placeholder = favoritesMenu_->addAction(tr("(No favorites)"));
        placeholder->setEnabled(false);
        return;
    }

    for (const QString &name : names) {
        auto *action = favoritesMenu_->addAction(name);
        action->setData(name);  // 供右键删除时识别
        connect(action, &QAction::triggered, this, [this, name]() {
            emit favoriteTriggered(name);
        });
    }
}

bool FavoritesMenuController::eventFilter(QObject *obj, QEvent *event) {
    // 收藏菜单右键：弹出删除菜单
    if (obj == favoritesMenu_ && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::RightButton) {
            auto *menu = static_cast<QMenu*>(obj);
            QAction *act = menu->actionAt(me->pos());
            if (act) {
                const QString name = act->data().toString();
                if (!name.isEmpty()) {
                    QMenu ctx(menu);
                    auto *removeAct = ctx.addAction(tr("Remove Favorite"));
                    removeAct->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
                    const QAction *chosen = ctx.exec(me->globalPosition().toPoint());
                    if (chosen == removeAct) {
                        if (FavoriteManager::instance()->removeFavorite(name)) {
                            refresh();
                        }
                    }
                    return true;  // 事件已处理
                }
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

} // namespace fm
