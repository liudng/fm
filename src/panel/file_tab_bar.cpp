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
}

FileTabBar::FileTabBar(QWidget *parent)
    : QTabBar(parent) {
    setTabsClosable(true);
    setMovable(true);
    setExpanding(false);
    setElideMode(Qt::ElideNone);
    setUsesScrollButtons(true);
    setDocumentMode(true);

    // 关闭按钮触发关闭信号
    connect(this, &QTabBar::tabCloseRequested, this,
            [this](int index) { emit closeTabRequested(index); });
}

QString FileTabBar::elideTitle(const QString &folderName) {
    if (folderName.length() <= kMaxTitleChars) return folderName;
    return folderName.left(kMaxTitleChars - 1) + QStringLiteral("…");
}

void FileTabBar::setTabPath(int index, const QString &path) {
    if (index < 0 || index >= count()) return;
    QString folderName = QDir(path).dirName();
    if (folderName.isEmpty()) folderName = path;  // 根目录
    setTabText(index, elideTitle(folderName));
    setTabToolTip(index, path);
}

QRect FileTabBar::newTabButtonRect() const {
    // "+" 按钮位于最后一个选项卡右侧
    if (count() == 0) {
        return QRect(4, 0, kNewTabButtonWidth, height());
    }
    const QRect lastTabRect = tabRect(count() - 1);
    const int startX = lastTabRect.right() + 4;
    return QRect(startX, 0, kNewTabButtonWidth, lastTabRect.height());
}

bool FileTabBar::isNewTabButton(const QPoint &pos) const {
    return newTabButtonRect().contains(pos);
}

void FileTabBar::mousePressEvent(QMouseEvent *event) {
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

void FileTabBar::contextMenuEvent(QContextMenuEvent *event) {
    const int index = tabAt(event->pos());
    if (index >= 0) {
        emit contextMenuRequested(index, event->globalPos());
        event->accept();
    } else {
        QTabBar::contextMenuEvent(event);
    }
}

void FileTabBar::mouseMoveEvent(QMouseEvent *event) {
    QTabBar::mouseMoveEvent(event);
}

void FileTabBar::paintEvent(QPaintEvent *event) {
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
        const int x = rect.center().x() - pm.width() / 2;
        const int y = rect.center().y() - pm.height() / 2;
        p.drawPixmap(x, y, pm);
    }
}

} // namespace fm
