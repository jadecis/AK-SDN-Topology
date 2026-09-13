#include "Node.h"
#include "NetworkMap.h"

Node::~Node()
{
}

QPoint Node::getPosition() const
{
    return position;
}

void Node::setPosition(QPoint position)
{
    this->position = position;
}

void Node::select()
{
    isSelected = true;
}

void Node::unselect()
{
    isSelected = false;
}
