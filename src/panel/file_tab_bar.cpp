#include "file_tab_bar.h"

#include <QContextMenuEvent>
#include <QDir>
#include <QMouseEvent>
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

    // tabMoved 信号已由 QTabBar 提供
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

bool FileTabBar::isNewTabButton(const QPoint &pos) const {
    // "+" 按钮位于最后一个选项卡右侧
    if (count() == 0) {
        return pos.x() >= 4 && pos.x() <= 4 + kNewTabButtonWidth;
    }
    const QRect lastTabRect = tabRect(count() - 1);
    const int startX = lastTabRect.right() + 4;
    return pos.x() >= startX && pos.x() <= startX + kNewTabButtonWidth &&
           pos.y() >= 0 && pos.y() <= lastTabRect.bottom();
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

} // namespace fm
