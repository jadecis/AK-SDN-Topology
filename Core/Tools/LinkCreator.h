#ifndef LINKCREATOR_H
#define LINKCREATOR_H

#include "Tool.h"
#include "LinkCreator.h"
#include "ConnectionClassifier.h"

class NetworkMap;

class LinkCreator : public Tool
{
public:
    LinkCreator(NetworkMap *map);
    virtual void handleMouseUp(QPoint position);

private:
    bool canCreateLinkBetween(Node *node1, Node *node2);
    bool createLink(Node *node1, Node *node2);

private:
    NetworkMap *map;
    Node *tmpNode;
    ConnectionClassifier classifier;
};

#endif // LINKCREATOR_H
