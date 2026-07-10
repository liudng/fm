#include "favorite_manager.h"

#include "../core/config_manager.h"
#include "../core/session_state.h"

#include <QUrl>

namespace fm {

FavoriteManager *FavoriteManager::instance()
{
    static FavoriteManager inst;
    return &inst;
}

FavoriteManager::FavoriteManager(QObject *parent) : QObject(parent) {}

QString FavoriteManager::encodeName(const QString &name) const
{
    // percent-encoding：对 / = 等特殊字符编码
    return QString::fromUtf8(QUrl::toPercentEncoding(name, QByteArrayLiteral("")));
}

QString FavoriteManager::decodeName(const QString &encoded) const
{
    return QUrl::fromPercentEncoding(encoded.toUtf8());
}

QStringList FavoriteManager::favoriteNames() const
{
    auto *cfg = ConfigManager::instance();
    const QStringList encoded =
        cfg->value(QStringLiteral("Favorites"), QStringLiteral("groups")).toStringList();
    QStringList result;
    result.reserve(encoded.size());
    for (const QString &e : encoded) {
        result.append(decodeName(e));
    }
    return result;
}

bool FavoriteManager::addFavorite(const QString &name, const LayoutState &state)
{
    if (name.isEmpty()) return false;
    const QString encoded = encodeName(name);

    auto *cfg = ConfigManager::instance();
    QStringList groups =
        cfg->value(QStringLiteral("Favorites"), QStringLiteral("groups")).toStringList();
    if (groups.contains(encoded)) return false;

    groups.append(encoded);
    cfg->setValue(QStringLiteral("Favorites"), QStringLiteral("groups"), groups);
    cfg->setValue(QStringLiteral("Favorites/") + encoded, QStringLiteral("data"),
                  SessionState::serialize(state));
    emit favoritesChanged();
    return true;
}

bool FavoriteManager::removeFavorite(const QString &name)
{
    const QString encoded = encodeName(name);
    auto *cfg = ConfigManager::instance();
    QStringList groups =
        cfg->value(QStringLiteral("Favorites"), QStringLiteral("groups")).toStringList();
    if (!groups.removeAll(encoded)) return false;

    cfg->setValue(QStringLiteral("Favorites"), QStringLiteral("groups"), groups);
    cfg->setValue(QStringLiteral("Favorites/") + encoded, QStringLiteral("data"), QString());
    emit favoritesChanged();
    return true;
}

bool FavoriteManager::loadFavorite(const QString &name, LayoutState &outState) const
{
    const QString encoded = encodeName(name);
    auto *cfg = ConfigManager::instance();
    const QString data =
        cfg->value(QStringLiteral("Favorites/") + encoded, QStringLiteral("data")).toString();
    if (data.isEmpty()) return false;
    return SessionState::deserialize(data, outState);
}

} // namespace fm
