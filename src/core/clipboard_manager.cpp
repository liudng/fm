#include "clipboard_manager.h"

#include <QClipboard>
#include <QDataStream>
#include <QGuiApplication>
#include <QMimeData>

namespace fm {

namespace {
// 自定义 MIME 用于保存剪切模式标记
const char *kFmCutMime = "application/x-fm-cut";
} // namespace

ClipboardManager *ClipboardManager::instance()
{
    static ClipboardManager inst;
    return &inst;
}

ClipboardManager::ClipboardManager(QObject *parent) : QObject(parent)
{
    // 监听系统剪贴板变化
    connect(QGuiApplication::clipboard(), &QClipboard::changed, this,
            &ClipboardManager::clipboardChanged);
}

void ClipboardManager::setFiles(const QList<QUrl> &urls, Mode mode)
{
    urls_ = urls;
    mode_ = mode;
    syncToSystemClipboard();
    emit clipboardChanged();
}

bool ClipboardManager::isCut(const QUrl &url) const
{
    if (mode_ != Mode::Cut) return false;
    return urls_.contains(url);
}

void ClipboardManager::clearCutMarks()
{
    if (mode_ == Mode::Cut) {
        mode_ = Mode::Copy;
        // 不清除系统剪贴板内容（保留为复制源）
        // 但需要更新自定义 MIME 移除剪切标记
        syncToSystemClipboard();
        emit clipboardChanged();
    }
}

void ClipboardManager::syncToSystemClipboard()
{
    auto *clipboard = QGuiApplication::clipboard();
    auto *mime = new QMimeData;

    // URI 列表
    mime->setUrls(urls_);

    // 文本形式（便于粘贴到文本编辑器）
    QStringList lines;
    for (const QUrl &u : urls_) {
        lines << u.toString(QUrl::FullyEncoded);
    }
    mime->setText(lines.join(QLatin1Char('\n')));

    // 剪切标记
    if (mode_ == Mode::Cut) {
        QByteArray data(1, '\1');
        mime->setData(QString::fromLatin1(kFmCutMime), data);
    }

    clipboard->setMimeData(mime);
}

} // namespace fm
