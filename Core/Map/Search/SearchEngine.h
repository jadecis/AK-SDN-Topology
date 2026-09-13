#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include "Node.h"
#include "Link.h"
#include "Switch.h"

class SearchEngine
{
public:
    Node *findNode(const QList<Node*> nodes, QPoint position);
    Node *findNode(const QList<Switch *> switches, int groupId);
    Link *findLink(const QList<Link*> links, QPoint position);
    Switch *findSwitch(const QList<Switch*> switches, QString name);
    Switch *findSwitch(const QList<Switch*> switches, int groupId);
    QList<Node *> findNodes(const QList<Node*> nodes, QPoint topLeft, QPoint bottomRight);
    Node *findNode(const QList<Node*> nodes, QString name);
    Element *findElement(const QList<Element*> elements, QPoint position);
};

#endif // SEARCHENGINE_H
