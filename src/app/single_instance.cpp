#include "single_instance.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QDir>

namespace fm {

namespace {
constexpr auto kSocketName = "fm-qt-single-instance";
}

SingleInstance::SingleInstance(QObject *parent) : QObject(parent) {}

SingleInstance::~SingleInstance()
{
    if (locked_ && server_) {
        server_->close();
    }
}

bool SingleInstance::tryLock()
{
    // 尝试连接已存在的实例
    QLocalSocket socket;
    socket.connectToServer(kSocketName);
    if (socket.waitForConnected(100)) {
        // 已有实例在运行
        socket.disconnectFromServer();
        return false;
    }

    // 创建本地 server
    server_ = new QLocalServer(this);
    // 移除可能残留的旧 socket 文件
    {
        const QString sockName = kSocketName;
        QLocalServer::removeServer(sockName);
    }
    if (!server_->listen(kSocketName)) {
        // 监听失败：清理后认为已有实例
        delete server_;
        server_ = nullptr;
        return false;
    }

    connect(server_, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    locked_ = true;
    return true;
}

void SingleInstance::sendPaths(const QStringList &paths)
{
    QLocalSocket socket;
    socket.connectToServer(kSocketName);
    if (!socket.waitForConnected(1000)) return;

    // 用 \n 分隔路径
    QByteArray data = paths.join(QLatin1Char('\n')).toUtf8();
    socket.write(data);
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
}

void SingleInstance::onNewConnection()
{
    QLocalSocket *socket = server_->nextPendingConnection();
    if (!socket) return;

    socket->waitForReadyRead(500);
    const QByteArray data = socket->readAll();
    socket->deleteLater();

    const QStringList paths = QString::fromUtf8(data).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (!paths.isEmpty()) {
        emit pathsReceived(paths);
    }
}

} // namespace fm
