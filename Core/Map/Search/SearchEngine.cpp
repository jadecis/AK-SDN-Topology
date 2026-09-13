#include "SearchEngine.h"

Node *SearchEngine::findNode(const QList<Node *> nodes, QPoint position)
{
    foreach (Node *n, nodes)
    {
        bool widthMatch = abs(n->getPosition().x() - position.x()) <=
                              n->getSize().width() / 2;
        bool heightMatch = abs(n->getPosition().y() - position.y()) <=
                               n->getSize().height() / 2;
        if (widthMatch && heightMatch)
        {
            return n;
        }
    }
    return NULL;
}

Link *SearchEngine::findLink(const QList<Link *> links, QPoint position)
{
    foreach (Link *ln, links)
    {
        bool widthMatch = abs(ln->getPosition().x() - position.x()) <=
                              ln->getSize().width() / 2;
        bool heightMatch = abs(ln->getPosition().y() - position.y()) <=
                               ln->getSize().height() / 2;
        if (widthMatch && heightMatch)
        {
            return ln;
        }
    }
    return NULL;
}

Switch *SearchEngine::findSwitch(const QList<Switch *> switches, QString name)
{
    foreach (Switch *sw, switches)
    {
        if (QString::compare(sw->getName(), name) == 0)
        {
            return sw;
        }
    }
    return NULL;
}

Switch *SearchEngine::findSwitch(const QList<Switch *> switches, int groupId)
{
    foreach (Switch *sw, switches)
    {
        if (sw->getGroupId() == groupId)
        {
            return sw;
        }
    }
    return NULL;
}

QList<Node *> SearchEngine::findNodes(const QList<Node *> nodes, QPoint topLeft, QPoint bottomRight)
{
    QList<Node *> findedNodes;
    foreach (Node *n, nodes)
    {
        bool widthMatch = n->getPosition().x() > topLeft.x() &&
                          n->getPosition().x() < bottomRight.x();
        bool heightMatch = n->getPosition().y() > topLeft.y() &&
                           n->getPosition().y() < bottomRight.y();
        if (widthMatch && heightMatch)
        {
            findedNodes.append(n);
        }
    }
    return findedNodes;
}

Node *SearchEngine::findNode(const QList<Node *> nodes, QString name)
{
    foreach (Node *n, nodes)
    {
        if (QString::compare(n->getName(), name) == 0)
        {
            return n;
        }
    }
    return NULL;
}

Node *SearchEngine::findNode(const QList<Switch *> switches, int groupId)
{
    foreach (Switch *sw, switches)
    {
        if (sw->getGroupId() == groupId)
        {
            return sw;
        }
    }
    return NULL;
}
