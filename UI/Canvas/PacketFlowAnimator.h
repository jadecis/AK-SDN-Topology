#ifndef PACKETFLOWANIMATOR_H
#define PACKETFLOWANIMATOR_H

#include <QObject>
#include <QList>
#include <QPointF>
#include <QColor>
#include <QTimer>

class TopologyScene;
class QGraphicsEllipseItem;
class LinkItem;

/* PacketFlowAnimator — рисует поток светящихся точек, бегущих по
 * сегментам маршрута. Не закрывает подсветку линий — точки рисуются
 * поверх (zValue=20), сами линии остаются видимы.
 *
 * Использование:
 *   animator->playOnce(linkSequence, color);   // одиночный «пакет»
 *   animator->startStream(linkSequence, color, intervalMs);  // непрерывный поток
 *   animator->stopStream(streamId);
 *   animator->clearAll();
 *
 * Цвет: pings → зелёный, iperf → синий, алгоритмы → пурпурный. */
class PacketFlowAnimator : public QObject
{
    Q_OBJECT
public:
    explicit PacketFlowAnimator(TopologyScene *scene);
    ~PacketFlowAnimator();

    /* Однократно отправить один пакет (точку) по маршруту. */
    void playOnce(const QList<LinkItem *> &path, QColor color = QColor("#22C55E"),
                  int durationMs = 1200);

    /* Запустить непрерывный поток точек. Возвращает streamId. */
    int startStream(const QList<LinkItem *> &path, QColor color = QColor("#22C55E"),
                    int intervalMs = 400, int durationMs = 1200);

    /* Остановить поток по id. */
    void stopStream(int streamId);

    /* Очистить все активные анимации/потоки/частицы. */
    void clearAll();

private slots:
    void onTick();

private:
    struct Particle {
        QGraphicsEllipseItem *item;
        QList<QPointF> waypoints;   // последовательные точки маршрута (scene-coords)
        int segmentIndex;           // текущий сегмент (0..waypoints.size()-2)
        qreal t;                    // [0..1] позиция в сегменте
        qreal segmentSpeed;         // прирост t за тик
        QColor color;
    };

    struct Stream {
        int id;
        QList<QPointF> waypoints;
        QColor color;
        int intervalMs;
        int durationMs;
        int sinceLastEmitMs;
        bool active;
    };

    TopologyScene *scene_;
    QTimer ticker_;
    int tickMs_;
    QList<Particle> particles_;
    QList<Stream> streams_;
    int nextStreamId_;

    QList<QPointF> waypointsFromPath(const QList<LinkItem *> &path) const;
    void spawnParticle(const QList<QPointF> &waypoints, QColor color, int durationMs);
};

#endif // PACKETFLOWANIMATOR_H
