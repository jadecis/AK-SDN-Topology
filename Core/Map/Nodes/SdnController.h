#ifndef SDNCONTROLLER_H
#define SDNCONTROLLER_H

#include "Node.h"
#include <QString>

class SdnController : public Node
{
public:
    SdnController(QPoint);
    ~SdnController();
    inline QString getPort() const;
    inline void setPort(QString port);
    inline QString getIp() const;
    inline void setIp(QString ip);

    virtual void configure();
    virtual void setup(int num);

    virtual void draw(NetworkMapDrawer *drawer);
    virtual DeviceType getDeviceType();
    virtual QSize getSize() const;

private:
    QString ip;
    QString port;
};

inline QString SdnController::getPort() const
{
    return port;
}

inline void SdnController::setPort(QString port)
{
    this->port = port;
}

inline QString SdnController::getIp() const
{
    return ip;
}

inline void SdnController::setIp(QString ip)
{
    this->ip = ip;
}

#endif // SDNCONTROLLER_H
