#include "Link.h"
#include "NetworkMap.h"
#include "Node.h"

Link::Link(Node *node1, Node *node2) : node1(node1), node2(node2)
{

}

Link::~Link()
{

}

QPoint Link::getPosition() const
{
    int x = (getNode1()->getPosition().x() + getNode2()->getPosition().x()) / 2;
    int y = (getNode1()->getPosition().y() + getNode2()->getPosition().y()) / 2;
    return QPoint(x, y);
}
