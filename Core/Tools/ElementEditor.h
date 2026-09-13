#ifndef ELEMENTEDITOR_H
#define ELEMENTEDITOR_H

#include "Tool.h"
#include "NetworkMap.h"

class ElementEditor : public Tool
{
public:
    ElementEditor(NetworkMap *map);
    virtual void handleMouseUp(QPoint);
    virtual void handleMouseDown(QPoint);
    virtual bool handleMouseMove(QPoint);
    virtual void handleMouseDoubleClicked(QPoint);
    virtual void handleKeyDeletePressEvent();
    void AlignVertically();
    void AlignHorizontally();

private:
    QList<Element *> selectedElements;
    QList<QSize> selectedElementsOffsets;
    NetworkMap *networkMap;
};

#endif // ELEMENTEDITOR_H
