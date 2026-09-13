#ifndef POXRECEIVER_H
#define POXRECEIVER_H

#include "QObject"
#include "TcpServer.h"

class CommandReceiver : public QObject
{
    Q_OBJECT

private:
    TcpServer *server;

public slots:
    void on();
    void handleData(QByteArray data);

signals:
    void signalPathReceived(QList<int> path);
    void signalIslandsReceived(QList<QList<int> > islands);
    void signalPathsReceived(QList<QList<int> > paths);
    void signalPTReceived(double pt);
    void signalMetricReceived(QVector<float> metricData);
    void signalMessageReceived(QString message);
    void signalTreeReceived(QList<QList<int>> tree);
    void signalTreesReceived(QList<QList<QList<int> > > trees);
};

#endif // POXRECEIVER_H
