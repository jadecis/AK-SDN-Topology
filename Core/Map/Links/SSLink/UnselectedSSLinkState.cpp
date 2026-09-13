#include "UnselectedSSLinkState.h"
#include "NetworkMapDrawer.h"

void UnselectedSSLinkState::stylize(NetworkMapDrawer *drawer, QPoint position1,
                                    QPoint position2)
{
    drawer->drawSolidLine(position1, position2);
}
