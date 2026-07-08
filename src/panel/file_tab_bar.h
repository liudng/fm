#ifndef FM_PANEL_FILE_TAB_BAR_H
#define FM_PANEL_FILE_TAB_BAR_H

#include <QTabBar>

namespace fm {

// 选项卡栏
// - 标题：当前文件夹名（最多 16 字符，超出截断前 15 + "…"）
// - tooltip：完整路径
// - 支持同一面板内拖拽调整顺序
// - "+" 新建按钮、"×" 关闭按钮
class FileTabBar : public QTabBar {
    Q_OBJECT
public:
    explicit FileTabBar(QWidget *parent = nullptr);

    // 设置选项卡路径，自动更新标题与 tooltip
    void setTabPath(int index, const QString &path);

signals:
    void newTabRequested();
    void closeTabRequested(int index);
    void contextMenuRequested(int index, const QPoint &globalPos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    // 标题截断：超过 16 字符显示前 15 + "…"
    static QString elideTitle(const QString &folderName);

    // "+" 按钮区域检测
    bool isNewTabButton(const QPoint &pos) const;
    // 计算 "+" 按钮的可视矩形
    QRect newTabButtonRect() const;

    // 拖拽起点
    QPoint pressPos_;
    int dragTab_ = -1;
};

} // namespace fm

#endif // FM_PANEL_FILE_TAB_BAR_H
