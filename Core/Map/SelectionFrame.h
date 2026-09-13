#ifndef SELECTIONFRAME_H
#define SELECTIONFRAME_H

#include <QRect>
#include <QPoint>
#include "NetworkMapDrawer.h"

class SelectionFrame
{
public:
    SelectionFrame();
    inline void activate(QPoint point);
    inline void deactivate();
    inline void setDynamicPoint(QPoint point);
    inline bool isActive() const;
    QPoint getTopLeft();
    QPoint getBottomRight();
    void draw(NetworkMapDrawer *drawer);

private:
    QPoint staticPoint;
    QPoint dynamicPoint;
    QPoint deadPoint;
};

inline bool SelectionFrame::isActive() const
{
    return staticPoint != deadPoint;
}

inline void SelectionFrame::activate(QPoint point)
{
    staticPoint = dynamicPoint = point;
}

inline void SelectionFrame::deactivate()
{
    staticPoint = dynamicPoint = deadPoint;
}

inline void SelectionFrame::setDynamicPoint(QPoint point)
{
    dynamicPoint = point;
}

#endif // SELECTIONFRAME_H
