#include "open_with_manager.h"

#include "../core/config_manager.h"

#include <QProcess>
#include <QFileInfo>

namespace fm {

OpenWithManager *OpenWithManager::instance()
{
    static OpenWithManager inst;
    return &inst;
}

OpenWithManager::OpenWithManager(QObject *parent) : QObject(parent) {}

QString OpenWithManager::defaultApplication(const QString &mimeType) const
{
    auto *cfg = ConfigManager::instance();
    return cfg->value(QStringLiteral("OpenWith"), mimeType).toString();
}

void OpenWithManager::setDefaultApplication(const QString &mimeType, const QString &desktopFile)
{
    auto *cfg = ConfigManager::instance();
    cfg->setValue(QStringLiteral("OpenWith"), mimeType, desktopFile);
}

QString OpenWithManager::systemDefault(const QString &mimeType)
{
    // 调用 xdg-mime query default <mimeType> 返回 .desktop 文件名
    QProcess proc;
    proc.start(QStringLiteral("xdg-mime"),
               {QStringLiteral("query"), QStringLiteral("default"), mimeType});
    if (!proc.waitForFinished(3000)) return {};
    const QString out = proc.readAllStandardOutput().trimmed();
    return out;
}

} // namespace fm
