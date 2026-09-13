#include "SSLink.h"
#include "Node.h"
#include "LinkDialog.h"
#include "MininetScriptBuilder.h"
#include "XmlSerializer.h"
#include "NetworkMapDrawer.h"
#include "DrawingMode.h"

SSLink::SSLink(Node *node1, Node *node2) :
    Link(node1, node2),
    delay(1),
    bandwidth(1),
    packetLossRate(0),
    disabled_(false)
{
    unselect();
}

void SSLink::draw(NetworkMapDrawer *drawer, PortMatrix portMatrix)
{
    state->stylize(drawer, getNode1()->getPosition(), getNode2()->getPosition());
    drawer->drawMetric(DrawingMode::selectMetricOf(this),
                       getNode1()->getPosition(),
                       getNode2()->getPosition());
    if (DrawingMode::needShowPorts)
    {
        drawer->drawPortsOnLine(getNode1()->getPosition(),
                                getNode2()->getPosition(),
                                QString("(%0)").arg(portMatrix.getPortNumber(getNode1(), getNode2())),
                                QString("(%0)").arg(portMatrix.getPortNumber(getNode2(), getNode1())));
    }
}

QSize SSLink::getSize() const
{
    return QSize(20, 20);
}

void SSLink::configure()
{
    LinkDialog dialog(this);
    dialog.exec();
}

void SSLink::select()
{
    state = &selectedState;
}

void SSLink::unselect()
{
    state = &unselectedState;
}

void SSLink::includeInPath(QColor color)
{
    state = &includedInPathState;
    includedInPathState.setPathColor(color);
}

void SSLink::includeInPaths(QColor color)
{
    state = &includedInPathsState;
    includedInPathsState.setPathColor(color);
}

DeviceType SSLink::getDeviceType()
{
    return SSLINK;
}
