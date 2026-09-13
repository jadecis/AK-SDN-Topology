#include "CSLink.h"
#include "XmlSerializer.h"
#include "Node.h"
#include "NetworkMapDrawer.h"

CSLink::CSLink(Node *node1, Node *node2) :
    Link(node1, node2)
{
}

void CSLink::draw(NetworkMapDrawer *drawer)
{
    drawer->drawDashLine(getNode1()->getPosition(), getNode2()->getPosition());
}

QSize CSLink::getSize() const
{
    return QSize(10, 10);
}

DeviceType CSLink::getDeviceType()
{
    return CSLINK;
}
