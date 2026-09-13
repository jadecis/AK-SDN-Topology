#ifndef TRAFFICMONITOR_H
#define TRAFFICMONITOR_H

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QHash>
#include <QColor>

class MininetHttpClient;

/* TrafficMonitor — периодически опрашивает Mininet HTTP-сервер
 * (endpoint /traffic_events) и эмитит сигнал packetSeen(from, to, color)
 * на каждое замеченное событие.
 *
 * Используется для оживления топологии: пакеты, идущие из Mininet CLI,
 * xterm-ов хостов или GUI — все отображаются как анимация на канве. */
class TrafficMonitor : public QObject
{
    Q_OBJECT
public:
    explicit TrafficMonitor(MininetHttpClient *client, QObject *parent = nullptr);

    void start(int intervalMs = 500);
    void stop();
    bool isRunning() const { return timer_.isActive(); }

signals:
    /* Один пакет: from→to. color: зелёный — ICMP, синий — iperf/TCP. */
    void packetSeen(QString fromHost, QString toHost, QColor color);

private slots:
    void onTick();
    void onEvents(QVariantList events);

private:
    MininetHttpClient *client_;
    QTimer timer_;
    double lastTs_;
    /* Дебаунс — карта "src>dst:kind" → unix-ts последней анимации.
     * Не плодим анимации чаще ~1.5 сек на одну пару, иначе пинг -c 10
     * заваливает GUI бесконечной чередой одинаковых пакетов. */
    QHash<QString, double> lastAnimAt_;
};

#endif // TRAFFICMONITOR_H
