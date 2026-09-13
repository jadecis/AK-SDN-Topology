#include "NodeConnector.h"


QList<CSLink *> NodeConnector::connectSdnControllerTo(QList<Switch *> switches, SdnController *c)
{
    QList<CSLink *> links;
    foreach (Switch *sw, switches)
    {
        links.append(new CSLink(c, sw));
    }
    return links;
}
