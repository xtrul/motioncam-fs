#pragma once

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

class SingleApplication : public QApplication
{
    Q_OBJECT

public:
    SingleApplication(int &argc, char **argv) : QApplication(argc, argv)
    {
        m_serverName = "motioncam-fs";
        m_localServer = nullptr;
        m_isRunning = false;
    }

    bool isRunning() {
        return m_isRunning;
    }

    bool listen() {
        QLocalSocket socket;

        socket.connectToServer(m_serverName);
        if (socket.waitForConnected()) {
            m_isRunning = true;
            // Another instance is already running
            return false;
        }

        m_localServer = new QLocalServer(this);
        connect(m_localServer, &QLocalServer::newConnection, this, &SingleApplication::receiveMessage);

        if (!m_localServer->listen(m_serverName)) {
            if (m_localServer->serverError() == QAbstractSocket::AddressInUseError) {
                QLocalServer::removeServer(m_serverName);
                m_localServer->listen(m_serverName);
            }
        }

        return true;
    }

    bool sendMessage(const QString &message) {
        QLocalSocket socket;

        socket.connectToServer(m_serverName);
        if (!socket.waitForConnected(3000)) {
            return false;
        }

        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);

        stream << message;
        socket.write(data);
        socket.waitForBytesWritten(3000);
        socket.disconnectFromServer();

        return true;
    }

signals:
    void messageReceived(const QString &message);

private slots:
    void receiveMessage()
    {
        QLocalSocket *socket = m_localServer->nextPendingConnection();
        if (!socket) return;

        connect(socket, &QLocalSocket::readyRead, [this, socket]() {
            QByteArray data = socket->readAll();
            QDataStream stream(data);
            QString message;

            stream >> message;
            emit messageReceived(message);
            socket->deleteLater();
        });
    }

private:
    QString m_serverName;
    QLocalServer *m_localServer;
    bool m_isRunning;
};
