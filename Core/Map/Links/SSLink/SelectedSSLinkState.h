#ifndef SELECTEDSSLINKSTATE_H
#define SELECTEDSSLINKSTATE_H

#include "SSLinkState.h"

class SelectedSSLinkState : public SSLinkState
{
public:
    virtual void stylize(NetworkMapDrawer *drawer, QPoint position1,
                         QPoint position2);
};

#endif // SELECTEDSSLINKSTATE_H
