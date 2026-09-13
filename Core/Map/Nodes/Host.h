#ifndef HOST_H
#define HOST_H

#include "Node.h"

class Host : public Node
{
public:
    Host(QPoint position);
    ~Host();
    inline QString getMac() const;
    inline void setMac(QString mac);
    inline QString getIp() const;
    inline void setIp(QString ip);

    virtual void configure();
    virtual void setup(int num);

    virtual void draw(NetworkMapDrawer *drawer);
    virtual DeviceType getDeviceType();
    virtual QSize getSize() const;

private:
    QString mac;
    QString ip;
};

inline QString Host::getMac() const
{
    return mac;
}

inline void Host::setMac(QString mac)
{
    this->mac = mac;
}

inline QString Host::getIp() const
{
    return ip;
}

inline void Host::setIp(QString ip)
{
    this->ip = ip;
}

#endif // HOST_H
