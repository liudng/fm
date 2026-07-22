#include "file_tab_bar.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

namespace fm {

namespace {
constexpr int kMaxTitleChars = 16;
constexpr int kNewTabButtonWidth = 28;
} // namespace

FileTabBar::FileTabBar(QWidget *parent) : QTabBar(parent)
{
    setMovable(true);
    setExpanding(false);
    setElideMode(Qt::ElideNone);
    setUsesScrollButtons(true);
    setDocumentMode(true);
    // 允许 tab bar 获取焦点，使点击非活动面板的选项卡时能通过
    // QApplication::focusChanged 触发活动面板切换
    setFocusPolicy(Qt::ClickFocus);

    // 关闭按钮触发关闭信号（是否显示由 PanelWidget 根据配置控制）
    connect(this, &QTabBar::tabCloseRequested, this,
            [this](int index) { emit closeTabRequested(index); });
}

QString FileTabBar::elideTitle(const QString &folderName)
{
    if (folderName.length() <= kMaxTitleChars) return folderName;
    return folderName.left(kMaxTitleChars - 1) + QStringLiteral("…");
}

void FileTabBar::setTabPath(int index, const QString &path)
{
    if (index < 0 || index >= count()) return;
    QString folderName = QDir(path).dirName();
    if (folderName.isEmpty()) folderName = path; // 根目录
    setTabText(index, elideTitle(folderName));
    setTabToolTip(index, path);
}

void FileTabBar::setActive(bool active)
{
    if (active_ == active) return;
    active_ = active;
    update();
}

QRect FileTabBar::newTabButtonRect() const
{
    // "+" 按钮位于最后一个选项卡右侧，垂直居中于选项卡行
    if (count() == 0) {
        const int btnH = kNewTabButtonWidth;
        const int y = (height() - btnH) / 2;
        return QRect(4, y, kNewTabButtonWidth, btnH);
    }
    const QRect lastTabRect = tabRect(count() - 1);
    const int startX = lastTabRect.right() + 4;
    // 与选项卡行垂直对齐：使用选项卡的 y 和 height 作为居中基准
    return QRect(startX, lastTabRect.y(), kNewTabButtonWidth, lastTabRect.height());
}

bool FileTabBar::isNewTabButton(const QPoint &pos) const
{
    return newTabButtonRect().contains(pos);
}

void FileTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // "+" 新建按钮
        if (isNewTabButton(event->pos())) {
            emit newTabRequested();
            event->accept();
            return;
        }
        pressPos_ = event->pos();
        dragTab_ = tabAt(event->pos());
    }
    QTabBar::mousePressEvent(event);
}

void FileTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int index = tabAt(event->pos());
    if (index >= 0) {
        emit contextMenuRequested(index, event->globalPos());
        event->accept();
    } else {
        QTabBar::contextMenuEvent(event);
    }
}

void FileTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
}

void FileTabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);

    // 在最后一个选项卡右侧绘制 "+" 新建按钮图标
    // 使用标准图标名 tab-new（兼容 gnome-icon-theme）
    const QRect rect = newTabButtonRect();
    QPainter p(this);
    const QIcon icon = QIcon::fromTheme(QStringLiteral("tab-new"));
    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    const QSize sz(iconSize, iconSize);
    const QPixmap pm = icon.pixmap(sz, isEnabled() ? QIcon::Normal : QIcon::Disabled);
    if (!pm.isNull()) {
        // 使用逻辑像素尺寸进行居中计算，避免高 DPI 下 pixmap 物理尺寸偏大导致图标偏上
        const qreal dpr = pm.devicePixelRatio();
        const int pmW = dpr > 0 ? qRound(pm.width() / dpr) : pm.width();
        const int pmH = dpr > 0 ? qRound(pm.height() / dpr) : pm.height();
        const int x = rect.center().x() - pmW / 2;
        const int y = rect.center().y() - pmH / 2;
        p.drawPixmap(x, y, pmW, pmH, pm);
    }

    // 活动面板：在选项卡栏顶部绘制高亮线
    if (active_) {
        const QColor highlight = palette().color(QPalette::Highlight);
        p.fillRect(QRect(0, 0, width(), 2), highlight);
    }
}

} // namespace fm
