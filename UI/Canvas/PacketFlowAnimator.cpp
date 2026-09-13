#include "PacketFlowAnimator.h"
#include "TopologyScene.h"
#include "LinkItem.h"
#include "NodeItem.h"

#include <QGraphicsEllipseItem>
#include <QBrush>
#include <QPen>

PacketFlowAnimator::PacketFlowAnimator(TopologyScene *scene) :
    QObject(scene),
    scene_(scene),
    tickMs_(33),         /* ~30 fps */
    nextStreamId_(1)
{
    ticker_.setInterval(tickMs_);
    connect(&ticker_, &QTimer::timeout, this, &PacketFlowAnimator::onTick);
}

PacketFlowAnimator::~PacketFlowAnimator()
{
    clearAll();
}

QList<QPointF> PacketFlowAnimator::waypointsFromPath(const QList<LinkItem *> &path) const
{
    /* Превращаем последовательность линков в список точек: A → B → C → ...
     * Логика: первая точка — node1 первого линка; последующие — каждый раз
     * берём узел, общий со следующим линком (или второй узел последнего линка). */
    QList<QPointF> pts;
    if (path.isEmpty()) return pts;

    auto nodePos = [](NodeItem *n) -> QPointF {
        return n ? n->pos() : QPointF();
    };

    if (path.size() == 1)
    {
        LinkItem *l = path[0];
        if (!l || !l->nodeA() || !l->nodeB()) return pts;
        pts << nodePos(l->nodeA()) << nodePos(l->nodeB());
        return pts;
    }

    /* >= 2 линков: ищем «общий» узел между парами. */
    NodeItem *prevEnd = nullptr;
    for (int i = 0; i < path.size(); ++i)
    {
        LinkItem *cur = path[i];
        if (!cur || !cur->nodeA() || !cur->nodeB()) return QList<QPointF>();

        if (i == 0)
        {
            LinkItem *nxt = path[i + 1];
            if (!nxt) return QList<QPointF>();
            NodeItem *startNode;
            NodeItem *common;
            if (cur->nodeA() == nxt->nodeA() || cur->nodeA() == nxt->nodeB())
            {
                common = cur->nodeA();
                startNode = cur->nodeB();
            }
            else
            {
                common = cur->nodeB();
                startNode = cur->nodeA();
            }
            pts << nodePos(startNode) << nodePos(common);
            prevEnd = common;
        }
        else
        {
            NodeItem *next = (cur->nodeA() == prevEnd) ? cur->nodeB()
                                                       : cur->nodeA();
            pts << nodePos(next);
            prevEnd = next;
        }
    }
    return pts;
}

void PacketFlowAnimator::spawnParticle(const QList<QPointF> &waypoints,
                                       QColor color, int durationMs)
{
    if (waypoints.size() < 2 || !scene_) return;

    QGraphicsEllipseItem *dot = new QGraphicsEllipseItem(-5, -5, 10, 10);
    dot->setBrush(QBrush(color));
    dot->setPen(QPen(QColor(255, 255, 255, 180), 1.5));
    dot->setZValue(20);
    dot->setPos(waypoints[0]);
    scene_->addItem(dot);

    Particle p;
    p.item = dot;
    p.waypoints = waypoints;
    p.segmentIndex = 0;
    p.t = 0.0;
    /* segmentSpeed: за каждый тик добавляем долю сегмента так, чтобы
     * за durationMs пройти все сегменты. Считаем равные сегменты. */
    int segments = waypoints.size() - 1;
    qreal perSegment = qreal(durationMs) / segments;
    p.segmentSpeed = qreal(tickMs_) / perSegment;
    if (p.segmentSpeed <= 0.0) p.segmentSpeed = 0.05;
    p.color = color;
    particles_.append(p);

    if (!ticker_.isActive()) ticker_.start();
}

void PacketFlowAnimator::playOnce(const QList<LinkItem *> &path, QColor color,
                                   int durationMs)
{
    QList<QPointF> wps = waypointsFromPath(path);
    spawnParticle(wps, color, durationMs);
}

int PacketFlowAnimator::startStream(const QList<LinkItem *> &path, QColor color,
                                     int intervalMs, int durationMs)
{
    Stream s;
    s.id = nextStreamId_++;
    s.waypoints = waypointsFromPath(path);
    s.color = color;
    s.intervalMs = qMax(50, intervalMs);
    s.durationMs = qMax(200, durationMs);
    s.sinceLastEmitMs = s.intervalMs;   /* первый пакет — сразу */
    s.active = true;
    streams_.append(s);
    if (!ticker_.isActive()) ticker_.start();
    return s.id;
}

void PacketFlowAnimator::stopStream(int streamId)
{
    for (int i = 0; i < streams_.size(); ++i)
    {
        if (streams_[i].id == streamId)
        {
            streams_[i].active = false;
        }
    }
}

void PacketFlowAnimator::clearAll()
{
    for (Particle &p : particles_)
    {
        if (p.item && p.item->scene() == scene_)
        {
            scene_->removeItem(p.item);
        }
        delete p.item;
    }
    particles_.clear();
    streams_.clear();
    ticker_.stop();
}

void PacketFlowAnimator::onTick()
{
    /* 1. Эмитируем новые частицы из активных потоков. */
    for (int i = streams_.size() - 1; i >= 0; --i)
    {
        Stream &s = streams_[i];
        if (!s.active)
        {
            streams_.removeAt(i);
            continue;
        }
        s.sinceLastEmitMs += tickMs_;
        if (s.sinceLastEmitMs >= s.intervalMs && s.waypoints.size() >= 2)
        {
            spawnParticle(s.waypoints, s.color, s.durationMs);
            s.sinceLastEmitMs = 0;
        }
    }

    /* 2. Двигаем частицы. */
    for (int i = particles_.size() - 1; i >= 0; --i)
    {
        Particle &p = particles_[i];
        if (!p.item) { particles_.removeAt(i); continue; }
        if (p.segmentIndex >= p.waypoints.size() - 1)
        {
            /* Достигла финальной точки — снимаем со сцены. */
            if (p.item->scene() == scene_) scene_->removeItem(p.item);
            delete p.item;
            particles_.removeAt(i);
            continue;
        }
        p.t += p.segmentSpeed;
        while (p.t >= 1.0 && p.segmentIndex < p.waypoints.size() - 1)
        {
            p.t -= 1.0;
            p.segmentIndex++;
        }
        if (p.segmentIndex >= p.waypoints.size() - 1)
        {
            if (p.item->scene() == scene_) scene_->removeItem(p.item);
            delete p.item;
            particles_.removeAt(i);
            continue;
        }
        QPointF a = p.waypoints[p.segmentIndex];
        QPointF b = p.waypoints[p.segmentIndex + 1];
        QPointF cur = a + (b - a) * p.t;
        p.item->setPos(cur);
    }

    if (particles_.isEmpty() && streams_.isEmpty())
    {
        ticker_.stop();
    }
}
