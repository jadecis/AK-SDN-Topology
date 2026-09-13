#include "Host.h"
#include "MininetScriptBuilder.h"
#include "XmlSerializer.h"
#include "HostDialog.h"
#include "NetworkMapDrawer.h"

Host::Host(QPoint position)
{
    unselect();
    setPosition(position);
}

Host::~Host()
{
}

void Host::configure()
{
    HostDialog dialog(this);
    dialog.exec();
}

void Host::setup(int num)
{
    setName(QString("h%0").arg(num));
    setIp(QString("10.0.0.%0").arg(num));
    setMac(QString("00:00:00:00:00:%0").arg(num, 2, 10, QChar('0')));
}

void Host::draw(NetworkMapDrawer *drawer)
{
    QPoint position = getPosition();
    QString name = getName();
    QSize size = getSize();
    int textVerticalOffset = 5;
    drawer->drawImage(QPixmap(":images/host.png"), position);
    drawer->drawDeviceName(name, QPoint(position.x(), position.y() +
                                        size.height() / 2 +
                                        drawer->getTextSize(name).height() / 2 +
                                        textVerticalOffset));
    if (isSelected)
    {
        drawer->drawElementFrame(size, position);
    }
}

DeviceType Host::getDeviceType()
{
    return HOST;
}

QSize Host::getSize() const
{
    return QPixmap(":images/host.png").size();
}
