#include "CommandReceiver.h"
#include "PathDecoder.h"
#include "IslandsDecoder.h"
#include "PathsDecoder.h"
#include "PairTransitionDecoder.h"
#include "MetricDecoder.h"
#include "TreeDecoder.h"
#include "TreesDecoder.h"

void CommandReceiver::on()
{
    server = new TcpServer(6111);
    connect(server, SIGNAL(signalDataReceived(QByteArray)),
            this, SLOT(handleData(QByteArray)));
    server->up();
}

void CommandReceiver::handleData(QByteArray data)
{
    QString command = QString(data);
    QStringList commandArgs = command.split("#");

    int name = 0, body = 1;
    if (QString::compare(commandArgs[name], QString("Path")) == 0)
    {
        PathDecoder decoder;
        QList<int> path = decoder.decode(commandArgs[body]);
        emit signalPathReceived(path);
    }
    else if (QString::compare(commandArgs[name], QString("Message")) == 0)
    {
        emit signalMessageReceived(commandArgs[body]);
    }
    else if (QString::compare(commandArgs[name], QString("Metric")) == 0)
    {
        MetricDecoder decoder;
        QVector<float> metricData = decoder.decode(commandArgs[body]);
        emit signalMetricReceived(metricData);
    }
    else if (QString::compare(commandArgs[name], QString("Tree")) == 0)
    {
        TreeDecoder decoder;
        QList<QList<int> > tree = decoder.decode(commandArgs[body]);
        emit signalTreeReceived(tree);
    }
    else if (QString::compare(commandArgs[name], QString("Trees")) == 0)
    {
        TreesDecoder decoder;
        QList<QList<QList<int> > > trees = decoder.decode(commandArgs[body]);
        emit signalTreesReceived(trees);
    }
    else if (QString::compare(commandArgs[name], QString("Paths")) == 0)
    {
        PathsDecoder decoder;
        QList<QList<int> > paths = decoder.decode(commandArgs[body]);
        emit signalPathsReceived(paths);
    }
    else if (QString::compare(commandArgs[name], QString("Islands")) == 0)
    {
        IslandsDecoder decoder;
        QList<QList<int> > islands = decoder.decode(commandArgs[body]);
        emit signalIslandsReceived(islands);
    }
    else if (QString::compare(commandArgs[name], QString("PT")) == 0)
    {
        PairTransitionDecoder decoder;
        double pt = decoder.decode(commandArgs[body]);
        emit signalPTReceived(pt);
    }

}
