#ifndef SIOYEK_ANDROID

#include "RunGuard.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QDebug>
#include <QTimer>

namespace
{
    QString generateKeyHash(const QString& key, const QString& salt)
    {
        QByteArray data;
        data.append(key.toUtf8());
        data.append(salt.toUtf8());
        data = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
        return data;
    }
}

RunGuard::RunGuard(const QString& key, bool defer_server) : QObject{},
key(key),
memoryKey(generateKeyHash(key, "_sharedMemKey"))
{
    // By explicitly attaching it and then deleting it we make sure that the
    // memory is deleted even after the process has crashed on Unix.
    memory = new QSharedMemory{ memoryKey };
    memory->attach();
    delete memory;

    // Guarantee process safe behaviour with a shared memory block.
    memory = new QSharedMemory(memoryKey);

    // Creates a shared memory segment then attaches to it with the given
    // access mode and returns true.
    // If a shared memory segment identified by the key already exists, the
    // attach operation is not performed and false is returned.

    //qDebug() << "Creating shared memory block...";
    if (memory->create(sizeof(quint64))) {
        //qDebug() << "Shared memory created: this is the primary application.";
        primary = true;
    }
    else {
        //qDebug() << "Shared memory already exists: this is a secondary application.";
        //qDebug() << "Secondary application attaching to shared memory block...";
        if (!memory->attach()) {
            qCritical() << "Secondary application cannot attach to shared memory block.";
            QCoreApplication::exit();
        }
        //qDebug() << "Secondary application successfully attached to shared memory block.";
    }

    memory->lock();
    if (primary && !defer_server) {
        if (!startServer()) {
            qCritical() << "Cannot start IPC server.";
            QCoreApplication::exit();
        }
    }
    memory->unlock();
}

RunGuard::~RunGuard()
{
    memory->lock();
    if (server) {
        server->close();
        delete server;
        server = nullptr;
    }
    memory->unlock();
    delete memory;
}

bool RunGuard::isPrimary()
{
    return primary;
}

bool RunGuard::isSecondary()
{
    return !primary;
}

bool RunGuard::startServer()
{
    if (!primary) return false;
    if (server) return server->isListening();
    QLocalServer::removeServer(key);
    server = new QLocalServer;
    server->setSocketOptions(QLocalServer::UserAccessOption);
    QObject::connect(server, &QLocalServer::newConnection, this, &RunGuard::onNewConnection);
    return server->listen(key);
}

void RunGuard::onNewConnection()
{
    QLocalSocket* socket = server->nextPendingConnection();
    QObject::connect(socket, &QLocalSocket::disconnected, this,
        [this, socket]() {
            if (on_delete) {
                on_delete(socket);
            }
            socket->deleteLater();
        }
    );
    QObject::connect(socket, &QLocalSocket::readyRead, this, [socket, this]() {
        readMessage(socket);
        });
}

void RunGuard::readMessage(QLocalSocket* socket)
{
    QByteArray data = socket->readAll();
    emit messageReceived(data, socket);
}

std::string RunGuard::sendMessage(const QByteArray& message, bool wait)
{
    QLocalSocket socket;
    // The primary holds its lock while initializing the GUI, before its final
    // event dispatcher can own the listener. Do not drop commands in that gap.
    QElapsedTimer startup_wait;
    startup_wait.start();
    do {
        socket.abort();
        socket.connectToServer(key, QLocalSocket::ReadWrite);
        if (socket.waitForConnected(100)) break;
        if (socket.error() != QLocalSocket::ServerNotFoundError &&
            socket.error() != QLocalSocket::ConnectionRefusedError) break;
        QThread::msleep(20);
    } while (startup_wait.elapsed() < 5000);
    if (socket.state() == QLocalSocket::ConnectedState) {
        if (socket.state() == QLocalSocket::ConnectedState) {
            socket.write(message);
            if (socket.waitForBytesWritten()) {
                if (wait) {
                    socket.waitForReadyRead();
                    std::string response = socket.readAll().toStdString();
                    return response;
                }
                //qCritical() << "Secondary application sent message to IPC server.";
            }
        }
    }
    else {
        qCritical() << "Secondary application cannot connect to IPC server.";
        qCritical() << "Socket error: " << socket.error();
        QCoreApplication::exit();
    }
    return "";
}

#endif // SIOYEK_ANDROID
