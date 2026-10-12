#pragma once
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <functional>
#include <memory>

// The lock elects the owner; the local socket carries Unicode file paths.
class SingleInstance : public QObject {
    QString name;
    QLockFile lock;
    QLocalServer server;
public:
    enum Result { Primary, Forwarded, Failed };
    std::function<void(QStringList)> received;
    explicit SingleInstance(const QString& key):name(key),lock(QDir::tempPath()+"/"+key+".lock") {}
    Result start(const QStringList& paths) {
        if(!lock.tryLock(0)) {
            QElapsedTimer elapsed; elapsed.start();
            while(elapsed.elapsed()<5000) {
                QLocalSocket socket; socket.connectToServer(name);
                if(socket.waitForConnected(100)) {
                    QJsonArray array; for(const auto& path:paths) array.append(path);
                    socket.write(QJsonDocument(array).toJson(QJsonDocument::Compact)+'\n'); socket.flush();
                    if(socket.bytesToWrite() && !socket.waitForBytesWritten(2000)) return Failed;
                    if(socket.bytesAvailable() || socket.waitForReadyRead(3000)) return socket.readAll().startsWith("OK") ? Forwarded : Failed;
                    return Failed;
                }
                QThread::msleep(50);
            }
            return Failed;
        }
        QLocalServer::removeServer(name);
        server.setSocketOptions(QLocalServer::UserAccessOption);
        if(!server.listen(name)) return Failed;
        connect(&server,&QLocalServer::newConnection,this,[this]{
            while(auto socket=server.nextPendingConnection()) {
                connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
                auto buffer=std::make_shared<QByteArray>();
                connect(socket,&QLocalSocket::readyRead,this,[this,socket,buffer]{
                    *buffer+=socket->readAll();
                    if(buffer->size()>1024*1024) { socket->disconnectFromServer(); return; }
                    if(!buffer->endsWith('\n')) return;
                    auto document=QJsonDocument::fromJson(*buffer);
                    if(!document.isArray()) { socket->disconnectFromServer(); return; }
                    QStringList paths; for(const auto& value:document.array()) if(value.isString()) paths.append(value.toString());
                    socket->write("OK\n"); socket->flush(); socket->disconnectFromServer();
                    QTimer::singleShot(0,this,[this,paths]{ if(received) received(paths); });
                });
            }
        });
        return Primary;
    }
};
