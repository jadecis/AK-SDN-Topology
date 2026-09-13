#ifndef UNSELECTEDSSLINKSTATE_H
#define UNSELECTEDSSLINKSTATE_H


#include "SSLinkState.h"

class UnselectedSSLinkState : public SSLinkState
{
public:
    virtual void stylize(NetworkMapDrawer *drawer, QPoint position1,
                         QPoint position2);
};

#endif // UNSELECTEDSSLINKSTATE_H
