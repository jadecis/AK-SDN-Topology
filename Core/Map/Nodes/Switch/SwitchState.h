#ifndef STATE_H
#define STATE_H

#include "NetworkMapDrawer.h"


class SwitchState
{
public:
    virtual void stylize(NetworkMapDrawer *drawer, QPoint position, QSize size) = 0;
};

#endif // STATE_H
