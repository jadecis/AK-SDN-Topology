#include "TrafficMonitor.h"
#include "MininetHttpClient.h"

#include <QVariantMap>

TrafficMonitor::TrafficMonitor(MininetHttpClient *client, QObject *parent) :
    QObject(parent),
    client_(client),
    lastTs_(0.0)
{
    timer_.setInterval(500);
    connect(&timer_, &QTimer::timeout, this, &TrafficMonitor::onTick);
    if (client_)
    {
        connect(client_, &MininetHttpClient::trafficEventsResult,
                this, &TrafficMonitor::onEvents);
    }
}

void TrafficMonitor::start(int intervalMs)
{
    timer_.setInterval(qMax(100, intervalMs));
    lastTs_ = 0.0;
    timer_.start();
}

void TrafficMonitor::stop()
{
    timer_.stop();
}

void TrafficMonitor::onTick()
{
    if (!client_) return;
    client_->pollTrafficEvents(lastTs_);
}

void TrafficMonitor::onEvents(QVariantList events)
{
    if (events.isEmpty()) return;
    /* Дебаунс: для пары (src,dst,kind) запускаем не чаще 1 анимации
     * каждые 1.5 секунды. Иначе ping -c 10 даёт 10+ накладывающихся
     * пакетов на одну линию (это и было видно у пользователя как
     * «бесконечно бегает по одному маршруту»). */
    const double kMinInterval = 1.5;
    for (const QVariant &v : events)
    {
        QVariantMap e = v.toMap();
        double ts = e.value("ts").toDouble();
        if (ts > lastTs_) lastTs_ = ts;
        QString src = e.value("src").toString();
        QString dst = e.value("dst").toString();
        QString kind = e.value("kind").toString();
        if (src.isEmpty() || dst.isEmpty()) continue;
        QString key = src + ">" + dst + ":" + kind;
        double prev = lastAnimAt_.value(key, 0.0);
        if (ts - prev < kMinInterval) continue;
        lastAnimAt_[key] = ts;
        QColor color = (kind == "iperf") ? QColor("#2563EB") : QColor("#22C55E");
        emit packetSeen(src, dst, color);
    }
    /* Чтобы карта дебаунса не пухла — обрезаем старое (> 2 минут). */
    if (lastAnimAt_.size() > 256)
    {
        const double cutoff = lastTs_ - 120.0;
        for (auto it = lastAnimAt_.begin(); it != lastAnimAt_.end(); )
        {
            if (it.value() < cutoff) it = lastAnimAt_.erase(it);
            else ++it;
        }
    }
}
