#include "SdnController.h"
#include "MininetScriptBuilder.h"
#include "NetworkMapDrawer.h"
#include "ControllerDialog.h"

SdnController::SdnController(QPoint position)
{
    unselect();
    setPosition(position);
}

SdnController::~SdnController()
{
}

void SdnController::configure()
{
    ControllerDialog dialog(this);
    dialog.exec();
}

void SdnController::setup(int num)
{
    setName(QString("c%0").arg(num));
    setIp("127.0.0.1");
    setPort("6653");
}

void SdnController::draw(NetworkMapDrawer *drawer)
{
    QPoint position = getPosition();
    QString name = getName();
    QSize size = getSize();
    int textVerticalOffset = 5;
    drawer->drawImage(QPixmap(":images/controller.png"), position);
    drawer->drawDeviceName(name, QPoint(position.x(), position.y() +
                                        size.height() / 2 +
                                        drawer->getTextSize(name).height() / 2 +
                                        textVerticalOffset));
    if (isSelected)
    {
        drawer->drawElementFrame(size, position);
    }
}

DeviceType SdnController::getDeviceType()
{
    return SDNCONTROLLER;
}

QSize SdnController::getSize() const
{
    return QPixmap(":images/controller.png").size();
}
