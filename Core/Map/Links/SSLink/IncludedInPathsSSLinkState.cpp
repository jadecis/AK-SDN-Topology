#include "IncludedInPathsSSLinkState.h"
#include "NetworkMapDrawer.h"


IncludedInPathsSSLinkState::IncludedInPathsSSLinkState()
{
    color = QColor("red");
}

void IncludedInPathsSSLinkState::stylize(NetworkMapDrawer *drawer, QPoint position1,
                                         QPoint position2)
{
    drawer->drawSolidLineInPaths(position1, position2, color);
}
