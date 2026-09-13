#include "SelectedSwitchState.h"

void SelectedSwitchState::stylize(NetworkMapDrawer *drawer, QPoint position, QSize size)
{
    drawer->drawElementFrame(size, position);
}
