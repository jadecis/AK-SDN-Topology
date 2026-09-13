#ifndef TOOL_H
#define TOOL_H

#include <QPoint>
#include "SearchEngine.h"

class Tool
{
public:
    virtual void handleMouseUp(QPoint) {}
    virtual void handleMouseDown(QPoint) {}
    virtual bool handleMouseMove(QPoint) { return false; }
    virtual void handleMouseDoubleClicked(QPoint) {}
    virtual void handleKeyDeletePressEvent() {}
};

#endif // TOOL_H
