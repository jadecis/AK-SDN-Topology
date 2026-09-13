#include "SelectionFrame.h"


SelectionFrame::SelectionFrame() : deadPoint(QPoint(-999, -999))
{
    deactivate();
}

QPoint SelectionFrame::getTopLeft()
{
    if (staticPoint.x() > dynamicPoint.x())
    {
        if (staticPoint.y() > dynamicPoint.y())
        {
            return dynamicPoint;
        }
        else
        {
            return QPoint(dynamicPoint.x(), staticPoint.y());
        }
    }
    else
    {
        if (staticPoint.y() > dynamicPoint.y())
        {
            return QPoint(staticPoint.x(), dynamicPoint.y());
        }
        else
        {
            return staticPoint;
        }
    }
}

QPoint SelectionFrame::getBottomRight()
{
    if (staticPoint.x() > dynamicPoint.x())
    {
        if (staticPoint.y() > dynamicPoint.y())
        {
            return staticPoint;
        }
        else
        {
            return QPoint(staticPoint.x(), dynamicPoint.y());
        }
    }
    else
    {
        if (staticPoint.y() > dynamicPoint.y())
        {
            return QPoint(dynamicPoint.x(), staticPoint.y());
        }
        else
        {
            return dynamicPoint;
        }
    }
}

void SelectionFrame::draw(NetworkMapDrawer *drawer)
{
    drawer->drawSelectionFrame(getTopLeft(), getBottomRight());
}
