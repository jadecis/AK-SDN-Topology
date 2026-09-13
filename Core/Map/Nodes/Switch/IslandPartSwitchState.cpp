#include "IslandPartSwitchState.h"

void IslandPartSwitchState::stylize(NetworkMapDrawer *drawer, QPoint position, QSize size)
{
    drawer->drawIslandFrame(position, QSize(size.width(), size.height()), color);
}
