#ifndef FM_APP_SINGLE_INSTANCE_H
#define FM_APP_SINGLE_INSTANCE_H

#include <QObject>
#include <QStringList>

class QLocalServer;

namespace fm {

// 单实例检测与跨进程通信
// - tryLock() 尝试绑定本地 socket
// - 已运行实例：sendPaths() 把命令行路径发送给运行中的实例
// - 运行实例：通过 pathsReceived 信号接收新调用
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;

    // 尝试获取锁，返回 true 表示是首个实例
    // false 表示已有实例运行（此时应调用 sendPaths 后退出）
    bool tryLock();

    // 发送路径列表给运行中的实例
    void sendPaths(const QStringList &paths);

signals:
    // 接收到新调用传来的路径列表
    void pathsReceived(const QStringList &paths);

private slots:
    void onNewConnection();

private:
    QLocalServer *server_ = nullptr;
    bool locked_ = false;
};

} // namespace fm

#endif // FM_APP_SINGLE_INSTANCE_H
