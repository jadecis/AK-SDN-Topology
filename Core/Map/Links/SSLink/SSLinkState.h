#ifndef SSLINKSTATE_H
#define SSLINKSTATE_H

class NetworkMapDrawer;
class QPoint;
class QColor;

class SSLinkState
{
public:
    virtual void stylize(NetworkMapDrawer *drawer, QPoint position1, QPoint position2) = 0;
};

#endif // SSLINKSTATE_H
