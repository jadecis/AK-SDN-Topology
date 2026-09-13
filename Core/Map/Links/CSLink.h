#ifndef CSLINK_H
#define CSLINK_H

#include "Link.h"

class CSLink : public Link
{
public:
    CSLink(Node *node1, Node *node2);
    inline ~CSLink();

    virtual void draw(NetworkMapDrawer *drawer);
    virtual QSize getSize() const;
    virtual DeviceType getDeviceType();
};

inline CSLink::~CSLink()
{
}

#endif // CSLINK_H
