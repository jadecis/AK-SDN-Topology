#ifndef DOCKERHEALTHMONITOR_H
#define DOCKERHEALTHMONITOR_H

#include <QObject>
#include <QTimer>
#include <QVariantList>

class MininetHttpClient;

/* DockerHealthMonitor — периодически опрашивает Mininet HTTP-сервер
 * (endpoint GET /docker/health) и эмитит healthUpdated() со списком
 * контейнеров и их состоянием.
 *
 * Используется для индикатора здоровья Docker-узлов на канве (#3):
 * цветной круг 🟢/🟡/🔴/⚫ на иконке. */
class DockerHealthMonitor : public QObject
{
    Q_OBJECT
public:
    explicit DockerHealthMonitor(MininetHttpClient *client, QObject *parent = nullptr);

    void start(int intervalMs = 3000);
    void stop();
    bool isRunning() const { return timer_.isActive(); }

signals:
    /* Каждый элемент: { name, image, status, health, healthy }. */
    void healthUpdated(QVariantList containers);

private slots:
    void onTick();
    void onResult(const QVariantList &containers);

private:
    MininetHttpClient *client_;
    QTimer timer_;
};

#endif // DOCKERHEALTHMONITOR_H
