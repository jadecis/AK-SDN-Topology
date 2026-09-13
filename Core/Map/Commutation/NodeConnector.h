#ifndef NODECONNECTOR_H
#define NODECONNECTOR_H

#include "CSLink.h"
#include "SdnController.h"
#include "Switch.h"

class NodeConnector
{
public:
    QList<CSLink *> connectSdnControllerTo(QList<Switch *> switches, SdnController *c);
};

#endif // NODECONNECTOR_H
