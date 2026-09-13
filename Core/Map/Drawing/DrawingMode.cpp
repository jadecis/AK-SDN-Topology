#include "DrawingMode.h"


bool DrawingMode::needShowPorts = false;
QString (*DrawingMode::selectMetricOf)(SSLink *ln) = NULL;


void DrawingMode::showDelay()
{
    selectMetricOf = [] (SSLink *ln)
    {
        return QString("%0ms").arg(ln->getDelay());
    };
}

void DrawingMode::showBandwidth()
{
    selectMetricOf = [] (SSLink *ln)
    {
        return QString("%0Mbit\\s").arg(ln->getBandwidth());
    };
}

void DrawingMode::showPacketLossRate()
{
    selectMetricOf = [] (SSLink *ln)
    {
        return QString("%0%").arg(ln->getPacketLoss());
    };
}
