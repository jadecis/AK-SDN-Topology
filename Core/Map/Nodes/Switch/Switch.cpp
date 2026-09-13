#include "Switch.h"
#include "MininetScriptBuilder.h"
#include "NetworkMapDrawer.h"
#include "XmlSerializer.h"

Switch::Switch(QPoint position)
{
    unselect();
    setPosition(position);
}

Switch::~Switch()
{
}

void Switch::setup(int num)
{
    setName(QString("s%0").arg(num));
}

void Switch::draw(NetworkMapDrawer *drawer)
{
    QPoint position = getPosition();
    QString name = getName();
    QSize size = getSize();
    int textVerticalOffset = 5;
    state->stylize(drawer, position, size);
    drawer->drawImage(QPixmap(":images/switch.png"), position);
    drawer->drawDeviceName(name, QPoint(position.x(), position.y() +
                                        size.height() / 2 +
                                        drawer->getTextSize(name).height() / 2 +
                                        textVerticalOffset));
}

DeviceType Switch::getDeviceType()
{
    return SWITCH;
}

QSize Switch::getSize() const
{
    return QPixmap(":images/switch.png").size();
}

void Switch::unselect()
{
    state = &unselectedState;
}

void Switch::select()
{
    state = &selectedState;
}

void Switch::includeInIsland(QColor color)
{
    islandPartState.setColor(color);
    state = &islandPartState;
}
