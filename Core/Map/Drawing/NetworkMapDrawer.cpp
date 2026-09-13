#include "NetworkMapDrawer.h"
#include <qmath.h>

NetworkMapDrawer::NetworkMapDrawer(QSize networkMapSize) :
    networkMapImage(networkMapSize)
{

}

void NetworkMapDrawer::beginDrawing()
{
    networkMapImage.fill();
    painter.begin(&networkMapImage);
}

QPixmap NetworkMapDrawer::getImage()
{
    painter.end();
    return networkMapImage;
}

void NetworkMapDrawer::drawImage(QPixmap image, QPoint position)
{
    int width = image.size().width();
    int height = image.size().height();
    painter.drawPixmap(position.x() - width / 2,
                       position.y() - height / 2,
                       image);
}

void NetworkMapDrawer::drawText(QString text, QPoint position, int size)
{
    QSize textSize = getTextSize(text);
    int x = position.x() - textSize.width() / 2;
    int y = position.y() - textSize.height() / 2;
    QRect textArea(QPoint(x, y), textSize);
    painter.setFont(QFont("Any", size));
    painter.setPen(Qt::black);
    painter.drawText(textArea, text);
}

void NetworkMapDrawer::drawDeviceName(QString name, QPoint position, int size)
{
    QString capitalisedName = name;
    capitalisedName[0] = capitalisedName[0].toUpper();
    drawText(capitalisedName, position, size);
}

void NetworkMapDrawer::drawRectangle(QPoint center, QSize size)
{
    QPoint topLeft(center.x() - size.width() / 2, center.y() - size.height() / 2);
    painter.fillRect(QRect(topLeft, size), Qt::white);
}

void NetworkMapDrawer::drawMetric(QString metric, QPoint position1, QPoint position2)
{
    QSize metricSize = getTextSize(metric);
    QPoint center = QPoint((position1.x() + position2.x()) / 2, (position1.y() + position2.y()) / 2);
    drawRectangle(center, metricSize);
    drawText(metric, center, 10);
}

void NetworkMapDrawer::drawSelectedText(QString text, QPoint position)
{
    QSize textSize = getTextSize(text);
    int x = position.x() - textSize.width() / 2;
    int y = position.y() - textSize.height() / 2;
    QRect textArea(QPoint(x, y), textSize);
    painter.setFont(QFont("Any", 11));
    painter.setPen(Qt::blue);
    painter.drawText(textArea, text);
}

void NetworkMapDrawer::drawElementFrame(QSize elementSize, QPoint position)
{
    int x = position.x();
    int y = position.y();
    int width = elementSize.width();
    int height = elementSize.height();
    painter.setPen(QPen(Qt::blue, 3, Qt::SolidLine));
    painter.drawRect(x - width / 2, y - height / 2, width, height);
}

void NetworkMapDrawer::drawSelectionFrame(QPoint topLeft, QPoint bottomRight)
{
    painter.setPen(QPen(Qt::blue, 3, Qt::SolidLine));
    painter.drawRect(QRect(topLeft, bottomRight));
}

void NetworkMapDrawer::drawSolidLine(QPoint position1, QPoint position2)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2, Qt::SolidLine));
    painter.drawLine(position1, position2);
}

void NetworkMapDrawer::drawSolidLineInPath(QPoint position1, QPoint position2, QColor color)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 4, Qt::SolidLine));
    painter.drawLine(position1, position2);
}

void NetworkMapDrawer::drawSolidLineInPaths(QPoint position1, QPoint position2, QColor color)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 8, Qt::SolidLine));
    painter.drawLine(position1, position2);
}

void NetworkMapDrawer::drawSelectedSolidLine(QPoint position1, QPoint position2)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::blue, 4, Qt::SolidLine));
    painter.drawLine(position1, position2);
}

void NetworkMapDrawer::drawDashLine(QPoint position1, QPoint position2)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
    painter.drawLine(position1, position2);
}

void NetworkMapDrawer::drawPortsOnLine(QPoint position1, QPoint position2, QString port1, QString port2)
{
    // y = kx + b, k = tg(alpha) = (y1 - y2) / (x1 - x2)
    // TODO: to handle division by zero
    double angle = qAtan((position1.y() - position2.y()) / (double)(position1.x() - position2.x()));
    QPoint port1Position;
    QPoint port2Position;
    int radius = 40;
    if (position1.x() < position2.x())
    {
        port1Position.setX(position1.x() + radius * qCos(angle));
        port1Position.setY(position1.y() + radius * qSin(angle));
        port2Position.setX(position2.x() - radius * qCos(angle));
        port2Position.setY(position2.y() - radius * qSin(angle));
    }
    else
    {
        port1Position.setX(position1.x() - radius * qCos(angle));
        port1Position.setY(position1.y() - radius * qSin(angle));
        port2Position.setX(position2.x() + radius * qCos(angle));
        port2Position.setY(position2.y() + radius * qSin(angle));
    }
    drawRectangle(port1Position, getTextSize(port1));
    drawText(port1, port1Position, 10);
    drawRectangle(port2Position, getTextSize(port2));
    drawText(port2, port2Position, 10);
}

void NetworkMapDrawer::drawIslandFrame(QPoint position, QSize size, QColor color)
{
    QPoint topLeft(position.x() - size.width() / 2, position.y() - size.height() / 2);
    painter.setPen(QPen(color, 30, Qt::SolidLine));
    painter.drawEllipse(QRect(topLeft, size));
}

QSize NetworkMapDrawer::getTextSize(QString text)
{
    QFontMetrics fontMetrics(QFont("Any", 11));
    return fontMetrics.size(Qt::TextExpandTabs, text);
}



