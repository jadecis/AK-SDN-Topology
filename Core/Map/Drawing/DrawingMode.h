#ifndef DRAWINGMODE_H
#define DRAWINGMODE_H

#include "SSLink.h"

class DrawingMode
{
public:
    static bool needShowPorts;
    static QString (*selectMetricOf)(SSLink *ln);
    static void showDelay();
    static void showBandwidth();
    static void showPacketLossRate();
};

#endif // DRAWINGMODE_H
