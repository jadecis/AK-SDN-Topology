#include "SelectedSSLinkState.h"
#include "NetworkMapDrawer.h"

void SelectedSSLinkState::stylize(NetworkMapDrawer *drawer, QPoint position1,
                                  QPoint position2)
{
    drawer->drawSelectedSolidLine(position1, position2);
}
