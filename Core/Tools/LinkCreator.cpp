#include "LinkCreator.h"
#include "NetworkMap.h"
#include "SSLink.h"
#include "CSLink.h"

LinkCreator::LinkCreator(NetworkMap *map) :
    map(map), tmpNode(NULL)
{
}

void LinkCreator::handleMouseUp(QPoint position)
{
    Node *node = map->getNodeByPosition(position);
    if (createLink(node, tmpNode))
    {
        tmpNode = NULL;
    }
    else
    {
        tmpNode = node;
    }
}

bool LinkCreator::canCreateLinkBetween(Node *node1, Node *node2)
{
    if (node1 == NULL || node2 == NULL)
    {
        return false;
    }
    bool areSameNodes = node1->getGlobalId() == node2->getGlobalId();
    bool node1ConnectedToNode2 = map->isConnected(node1, node2);
    return (areSameNodes || node1ConnectedToNode2) ? false : true;
}

bool LinkCreator::createLink(Node *node1, Node *node2)
{
    if (!canCreateLinkBetween(node1, node2))
    {
        return false;
    }
    else if (classifier.isSSConnection(node1, node2))
    {
        map->addSSLink(new SSLink(node1, node2));
        return true;
    }
    else if (classifier.isHSConnection(node1, node2))
    {
        map->addSSLink(new SSLink(node1, node2));
        return true;
    }
    else
    {
        return false;
    }

}
