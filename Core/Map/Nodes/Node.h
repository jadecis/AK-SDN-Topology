#ifndef NODE_H
#define NODE_H

#include "Element.h"
#include "Devicetype.h"

class Node : public Element
{
public:
    ~Node();
    virtual void setup(int num) {}
    virtual QPoint getPosition() const;
    virtual void setPosition(QPoint position);
    virtual void configure() {}
    inline virtual QString getName();
    inline virtual void setName(QString name);
    virtual void addDataIn(MininetScriptBuilder *) {}
    virtual void addDataIn(XmlSerializer *) {}
    virtual void select();
    virtual void unselect();

private:
    QString name;
    QPoint position;

protected:
    bool isSelected;
};

inline QString Node::getName()
{
    return name;
}

inline void Node::setName(QString name)
{
    this->name = name;
}

#endif // NODE_H
