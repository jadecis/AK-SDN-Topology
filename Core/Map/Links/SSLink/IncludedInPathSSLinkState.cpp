#include "IncludedInPathSSLinkState.h"
#include "NetworkMapDrawer.h"

IncludedInPathSSLinkState::IncludedInPathSSLinkState()
{
    color = QColor("red");
}

void IncludedInPathSSLinkState::stylize(NetworkMapDrawer *drawer, QPoint position1,
                                        QPoint position2)
{
    drawer->drawSolidLineInPath(position1, position2, color);
}
