#ifndef INCLUDEDINPATHSSLINKSTATE_H
#define INCLUDEDINPATHSSLINKSTATE_H

#include "SSLinkState.h"
#include "QColor"

class IncludedInPathSSLinkState : public SSLinkState
{
public:
    IncludedInPathSSLinkState();
    virtual void stylize(NetworkMapDrawer *drawer, QPoint position1,
                         QPoint position2);
    inline void setPathColor(QColor color);

private:
    QColor color;
};

inline void IncludedInPathSSLinkState::setPathColor(QColor color)
{
    this->color = color;
}

#endif // INCLUDEDINPATHSSLINKSTATE_H
