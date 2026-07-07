#ifndef FM_CORE_FAVORITE_MANAGER_H
#define FM_CORE_FAVORITE_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

namespace fm {

struct LayoutState;

// 收藏管理（单例）
// - 收藏项保存布局状态到 [Favorites/<encoded_name>]
// - 名称含特殊字符使用 percent-encoding（如 / → %2F，= → %3D）
// - 收藏列表保存在 [Favorites] groups 键
class FavoriteManager : public QObject {
    Q_OBJECT
public:
    static FavoriteManager *instance();

    // 获取所有收藏名称（解码后）
    QStringList favoriteNames() const;

    // 添加收藏：保存当前布局到 [Favorites/<name>]
    // 名称重名返回 false
    bool addFavorite(const QString &name, const LayoutState &state);

    // 删除收藏
    bool removeFavorite(const QString &name);

    // 加载收藏对应布局
    bool loadFavorite(const QString &name, LayoutState &outState) const;

signals:
    void favoritesChanged();

private:
    FavoriteManager(QObject *parent = nullptr);

    // 名称与配置 section 之间的转换
    QString encodeName(const QString &name) const;
    QString decodeName(const QString &encoded) const;
};

} // namespace fm

#endif // FM_CORE_FAVORITE_MANAGER_H
