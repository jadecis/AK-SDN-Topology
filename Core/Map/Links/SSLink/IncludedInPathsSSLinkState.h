#ifndef INCLUDEDINPATHSSSLINKSTATE_H
#define INCLUDEDINPATHSSSLINKSTATE_H

#include "SSLinkState.h"
#include <QColor>

class IncludedInPathsSSLinkState : public SSLinkState
{
public:
    IncludedInPathsSSLinkState();
    virtual void stylize(NetworkMapDrawer *drawer, QPoint position1,
                         QPoint position2);
    inline void setPathColor(QColor color);

private:
    QColor color;
};

inline void IncludedInPathsSSLinkState::setPathColor(QColor color)
{
    this->color = color;
}


#endif // INCLUDEDINPATHSSSLINKSTATE_H
