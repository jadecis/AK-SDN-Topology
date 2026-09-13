#ifndef SELECTEDSTATE_H
#define SELECTEDSTATE_H

#include "SwitchState.h"

class SelectedSwitchState : public SwitchState
{
public:
    virtual void stylize(NetworkMapDrawer *drawer, QPoint position, QSize size);
};

#endif // SELECTEDSTATE_H
