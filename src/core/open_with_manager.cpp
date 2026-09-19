#include "open_with_manager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace fm {

namespace {

// 用户级 applications 目录（XDG_DATA_HOME/applications）
QString userApplicationsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
}

// 判断 .desktop 文件是否位于系统/用户标准 applications 目录中
bool isInStandardApplicationsDir(const QString &desktopFile)
{
    const QString path = QDir::cleanPath(QFileInfo(desktopFile).absoluteFilePath());
    const auto dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const QString &dir : dirs) {
        const QString base = QDir::cleanPath(dir);
        if (path == base || path.startsWith(base + QDir::separator()))
            return true;
    }
    return false;
}

} // namespace

bool OpenWithManager::setDefaultApplication(const QString &mimeType, const QString &app)
{
    if (mimeType.isEmpty() || app.isEmpty())
        return false;

    const QString desktopName = registerApplication(app);
    if (desktopName.isEmpty())
        return false;

    return updateMimeappsDefault(mimeType, desktopName);
}

QString OpenWithManager::registerApplication(const QString &app)
{
    const QString userDir = userApplicationsDir();
    if (!QDir().mkpath(userDir))
        return {};

    if (app.endsWith(QStringLiteral(".desktop"))) {
        const QFileInfo fi(app);
        if (!fi.isFile())
            return {};
        // 标准目录中的 .desktop 直接引用文件名
        if (isInStandardApplicationsDir(app))
            return fi.fileName();
        // 非标准路径：复制到用户 applications 目录
        const QString dst = userDir + QDir::separator() + fi.fileName();
        if (QFile::exists(dst) && !QFile::remove(dst))
            return {};
        if (!QFile::copy(app, dst))
            return {};
        return fi.fileName();
    }

    // 自定义命令：生成 fm-custom-<hash>.desktop
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(app.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
    const QString name = QStringLiteral("fm-custom-%1.desktop").arg(hash);

    // Exec 行：字面 % 转义为 %%，字段代码 %f/%F/%u/%U 保留；
    // 命令中无字段代码时追加 " %f" 以接收文件路径
    QString exec = app;
    const bool hasFieldCode = exec.contains(QLatin1String("%f")) ||
                              exec.contains(QLatin1String("%F")) ||
                              exec.contains(QLatin1String("%u")) ||
                              exec.contains(QLatin1String("%U"));
    exec.replace(QStringLiteral("%"), QStringLiteral("%%"));
    if (hasFieldCode) {
        exec.replace(QStringLiteral("%%f"), QStringLiteral("%f"));
        exec.replace(QStringLiteral("%%F"), QStringLiteral("%F"));
        exec.replace(QStringLiteral("%%u"), QStringLiteral("%u"));
        exec.replace(QStringLiteral("%%U"), QStringLiteral("%U"));
    } else {
        exec += QStringLiteral(" %f");
    }

    // Name 取命令首行（.desktop 的 Name 不允许换行）
    QString display = app.section(QLatin1Char('\n'), 0, 0);
    if (display.isEmpty())
        display = QStringLiteral("fm custom command");

    QSaveFile f(userDir + QDir::separator() + name);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    f.write("[Desktop Entry]\n");
    f.write("Type=Application\n");
    f.write("Name=" + display.toUtf8() + "\n");
    f.write("Exec=" + exec.toUtf8() + "\n");
    f.write("NoDisplay=true\n");
    if (!f.commit())
        return {};

    return name;
}

bool OpenWithManager::updateMimeappsDefault(const QString &mimeType, const QString &desktopName)
{
    const QString path = QStandardPaths::writableLocation(
                             QStandardPaths::GenericConfigLocation) +
                         QDir::separator() + QStringLiteral("mimeapps.list");
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    // 逐行合并，保留其它内容（含注释与未知 section）
    QStringList lines;
    QFile src(path);
    if (src.exists()) {
        if (!src.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QTextStream ts(&src);
        while (!ts.atEnd())
            lines.append(ts.readLine());
    }

    const QString entry = mimeType + QLatin1Char('=') + desktopName;
    const QString sectionHeader = QStringLiteral("[Default Applications]");

    bool inSection = false;
    bool written = false;
    QStringList out;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('['))) {
            if (inSection && !written) {
                // 目标 section 结束仍未找到该 MIME 类型：插入到 section 末尾
                out.append(entry);
                written = true;
            }
            inSection = (trimmed == sectionHeader);
            out.append(line);
            continue;
        }
        if (inSection && !written) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq > 0 && line.left(eq).trimmed() == mimeType) {
                out.append(entry);
                written = true;
                continue;
            }
        }
        out.append(line);
    }
    if (inSection && !written) {
        // 目标 section 位于文件末尾
        out.append(entry);
        written = true;
    }
    if (!written) {
        // 文件中无 [Default Applications] section：追加
        if (!out.isEmpty() && !out.last().isEmpty())
            out.append(QString());
        out.append(sectionHeader);
        out.append(entry);
    }

    QByteArray content;
    for (const QString &line : out) {
        content += line.toUtf8();
        content += '\n';
    }

    QSaveFile dst(path);
    if (!dst.open(QIODevice::WriteOnly))
        return false;
    dst.write(content);
    return dst.commit();
}

} // namespace fm
