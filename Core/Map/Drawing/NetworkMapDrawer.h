#ifndef NETWORKMAPDRAWER_H
#define NETWORKMAPDRAWER_H

#include <QPainter>
#include "PortMatrix.h"

class NetworkMapDrawer
{
public:
    NetworkMapDrawer(QSize networkMapSize);
    void beginDrawing();
    QPixmap getImage();
    QSize getTextSize(QString text);
    void drawImage(QPixmap image, QPoint position);
    void drawText(QString text, QPoint position, int size = 11);
    void drawDeviceName(QString name, QPoint position, int size = 11);
    void drawRectangle(QPoint center, QSize size);
    void drawMetric(QString metric, QPoint position1, QPoint position2);
    void drawSelectedText(QString text, QPoint position);
    void drawElementFrame(QSize elementSize, QPoint position);
    void drawSelectionFrame(QPoint topLeft, QPoint bottomRight);
    void drawSolidLine(QPoint position1, QPoint position2);
    void drawSolidLineInPath(QPoint position1, QPoint position2, QColor color);
    void drawSolidLineInPaths(QPoint position1, QPoint position2, QColor color);
    void drawSelectedSolidLine(QPoint position1, QPoint position2);
    void drawDashLine(QPoint position1, QPoint position2);
    void drawPortsOnLine(QPoint position1, QPoint position2, QString port1, QString port2);
    void drawIslandFrame(QPoint position, QSize size, QColor color);

private:
    QPainter painter;
    QPixmap networkMapImage;
    PortMatrix portMtrix;
};

#endif // NETWORKMAPDRAWER_H
