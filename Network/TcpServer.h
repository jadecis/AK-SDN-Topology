#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QThread>

class QTcpServer;

class TcpServer : public QThread
{
    Q_OBJECT

private:
    QTcpServer *core;
    bool online;

public:
    TcpServer(int listenedPort);
    ~TcpServer();
    virtual void up();

public slots:
    void handleNewConnection();
    void readData();

signals:
    void signalConnectionClosed();
    void signalDataReceived(QByteArray);
};

#endif // TCPSERVER_H
