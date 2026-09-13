#include "DockerHealthMonitor.h"
#include "MininetHttpClient.h"

DockerHealthMonitor::DockerHealthMonitor(MininetHttpClient *client, QObject *parent) :
    QObject(parent),
    client_(client)
{
    timer_.setInterval(3000);
    connect(&timer_, &QTimer::timeout, this, &DockerHealthMonitor::onTick);
    if (client_)
    {
        connect(client_, &MininetHttpClient::dockerHealthResult,
                this, &DockerHealthMonitor::onResult);
    }
}

void DockerHealthMonitor::start(int intervalMs)
{
    timer_.setInterval(qMax(1000, intervalMs));
    timer_.start();
}

void DockerHealthMonitor::stop()
{
    timer_.stop();
}

void DockerHealthMonitor::onTick()
{
    if (!client_) return;
    client_->fetchDockerHealth();
}

void DockerHealthMonitor::onResult(const QVariantList &containers)
{
    emit healthUpdated(containers);
}
