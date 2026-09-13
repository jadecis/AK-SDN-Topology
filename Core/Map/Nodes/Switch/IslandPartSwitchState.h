#ifndef ISLANDPARTSTATE_H
#define ISLANDPARTSTATE_H

#include "SwitchState.h"

class IslandPartSwitchState : public SwitchState
{
public:
    virtual void stylize(NetworkMapDrawer *drawer, QPoint position, QSize size);
    inline void setColor(QColor color);

private:
    QColor color;
};

inline void IslandPartSwitchState::setColor(QColor color)
{
    this->color = color;
}

#endif // ISLANDPARTSTATE_H
