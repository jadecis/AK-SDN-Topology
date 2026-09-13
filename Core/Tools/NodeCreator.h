#ifndef NODECREATOR_H
#define NODECREATOR_H

#include "Tool.h"

class NetworkMap;

class NodeCreator : public Tool
{
public:
    NodeCreator(NetworkMap *map);
    virtual void handleMouseDown(QPoint);
    void (*createNode)(NetworkMap *map, QPoint position);
    void setSdnControllerCreation();
    void setSwitchCreation();
    void setHostCreation();
    void setTextLabelCreation();

private:
    NetworkMap *map;

};

#endif // NODECREATOR_H
