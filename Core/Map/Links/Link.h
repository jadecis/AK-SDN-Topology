/* Link:
 * - sets link parameters
 */

#ifndef LINK_H
#define LINK_H

#include "Element.h"

class Node;
class NetworkMap;

class Link : public Element
{
public:
    Link(Node *node1, Node *node2);
    ~Link();

    Node *getNode1() const;
    Node *getNode2() const;
    virtual QPoint getPosition() const;
    virtual void setPosition(QPoint) {}
    virtual void configure() {}
    virtual QString getName() { return ""; }
    virtual void setName(QString) {}
    virtual void select() {}
    virtual void unselect() {}

private:
    Node *node1;
    Node *node2;
};

inline Node *Link::getNode1() const
{
    return node1;
}

inline Node *Link::getNode2() const
{
    return node2;
}

#endif // LINK_H
