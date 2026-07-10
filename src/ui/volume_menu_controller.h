#ifndef FM_UI_VOLUME_MENU_CONTROLLER_H
#define FM_UI_VOLUME_MENU_CONTROLLER_H

#include "../core/volume_manager.h"

#include <QList>
#include <QObject>
#include <QPointer>

class QMenu;
class QAction;

namespace fm {

// 文件菜单卷段控制器
// - 在文件菜单中构建"已挂载卷 + 外部设备"两段（分隔符分隔）
// - aboutToShow 时枚举：卷段同步（QStorageInfo，快），外部设备段异步（UDisks2，慢）
//   外部设备段先显示"加载中"占位，后台枚举完成后替换
// - 右键弹上下文菜单执行挂载/卸载/弹出（QtConcurrent 异步，不阻塞 UI）
// - 左键点击已挂载卷项通过 navigateRequested 信号通知宿主导航
class VolumeMenuController : public QObject
{
    Q_OBJECT
public:
    explicit VolumeMenuController(QMenu *fileMenu, QObject *parent = nullptr);

    // 创建卷段/外部设备段两个分隔符，连接 aboutToShow，安装 eventFilter。
    // 调用后宿主可在 extSeparator_ 之后追加 Quit 等菜单项。
    void setup();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void refresh();
    void fillExternalDevices(const QList<VolumeInfo> &devices);

signals:
    // 左键点击已挂载卷项，请求宿主在活动面板导航到该挂载点
    void navigateRequested(const QString &mountPoint);
    // 请求宿主在状态栏显示瞬时消息（卷操作进行中提示等）
    void statusMessageRequested(const QString &msg, int timeout);
    // 卷操作失败，请求宿主弹出错误对话框（宿主可作为对话框父窗口）
    void operationFailed(const QString &errorMsg);

private:
    // 右键卷项/外部设备项：弹挂载/卸载/弹出上下文菜单
    void showContextMenu(QAction *act, const QPoint &globalPos);

    QMenu *fileMenu_;
    QAction *volSeparator_ = nullptr; // 卷段与外部设备段之间
    QAction *extSeparator_ = nullptr; // 外部设备段与后续项之间
    QList<QAction *> volActions_;     // 动态卷项（aboutToShow 时刷新）
    QList<QAction *> extActions_;     // 动态外部设备项（异步填充）
};

} // namespace fm

#endif // FM_UI_VOLUME_MENU_CONTROLLER_H
