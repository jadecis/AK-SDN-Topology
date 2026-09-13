#ifndef ELEMENT_H
#define ELEMENT_H

#include <QPainter>
#include "Devicetype.h"

class NetworkMap;
class MininetScriptBuilder;
class XmlSerializer;
class Graph;
class NetworkMapDrawer;

class Element
{
private:
    int groupId;
    int globalId;

public:
    Element();
    virtual ~Element();
    virtual QSize getSize() const = 0;
    virtual QPoint getPosition() const = 0;
    virtual void setPosition(QPoint position) = 0;
    virtual void configure() = 0;
    virtual void select() = 0;
    virtual void unselect() = 0;
    virtual QString getName() = 0;
    virtual void setName(QString name) = 0;
    virtual DeviceType getDeviceType() = 0;
    inline void setGroupId(int groupId);
    inline int getGroupId() const;
    inline void setGlobalId(int globalId);
    inline int getGlobalId() const;
};

inline void Element::setGroupId(int gid)
{
    this->groupId = gid;
}

inline int Element::getGroupId() const
{
    return groupId;
}

inline void Element::setGlobalId(int globalId)
{
    this->globalId = globalId;
}

inline int Element::getGlobalId() const
{
    return globalId;
}

#endif // ELEMENT_H
