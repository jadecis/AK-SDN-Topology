#ifndef UNSELECTEDSTATE_H
#define UNSELECTEDSTATE_H

#include "SwitchState.h"

class UnselectedSwitchState : public SwitchState
{
public:
    virtual void stylize(NetworkMapDrawer*, QPoint, QSize);
};

#endif // UNSELECTEDSTATE_H
