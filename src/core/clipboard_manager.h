#ifndef FM_CORE_CLIPBOARD_MANAGER_H
#define FM_CORE_CLIPBOARD_MANAGER_H

#include <QObject>
#include <QUrl>

namespace fm {

// 剪贴板管理（单例）
// - 复制/剪切文件路径以 URI 形式存入系统剪贴板（text/uri-list）
// - 维护剪切标记，区分复制/剪切
// - 标准剪贴板行为：再次剪切时旧剪切项恢复
class ClipboardManager : public QObject
{
    Q_OBJECT
public:
    enum class Mode
    {
        Copy,
        Cut
    };

    static ClipboardManager *instance();

    // 设置剪贴板内容（同时写入系统剪贴板以便跨应用粘贴）
    void setFiles(const QList<QUrl> &urls, Mode mode);
    QList<QUrl> files() const { return urls_; }
    Mode mode() const { return mode_; }
    bool hasFiles() const { return !urls_.isEmpty(); }

    // 查询单个 URL 是否处于剪切状态
    bool isCut(const QUrl &url) const;

    // 取消所有剪切标记（粘贴后调用）
    void clearCutMarks();

signals:
    // 剪贴板内容变更（含系统剪贴板外部变更）
    void clipboardChanged();

private:
    ClipboardManager(QObject *parent = nullptr);
    void syncToSystemClipboard();

    QList<QUrl> urls_;
    Mode mode_ = Mode::Copy;
};

} // namespace fm

#endif // FM_CORE_CLIPBOARD_MANAGER_H
