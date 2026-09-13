#include "DockerNode.h"
#include "NetworkMapDrawer.h"
#include "DockerDialog.h"

DockerNode::DockerNode(QPoint position)
{
    unselect();
    setPosition(position);
    /* ВАЖНО: Containernet после старта контейнера запускает внутри
     * `bash --norc -is` для приёма команд от Mininet CLI. Alpine
     * НЕ имеет bash (только /bin/sh BusyBox) → contаiner становится
     * недоступен из CLI. Поэтому default — ubuntu (там bash есть). */
    image_ = "ubuntu:22.04";
    command_ = "";  // используем default CMD/ENTRYPOINT образа
}

DockerNode::~DockerNode()
{
}

void DockerNode::configure()
{
    DockerDialog dialog(this);
    dialog.exec();
}

void DockerNode::setup(int num)
{
    /* Имя контейнера обязано быть валидным для Docker (только [a-z0-9_-]). */
    setName(QString("d%0").arg(num));
    /* IP с offset 100, чтобы не конфликтовать с хостами (10.0.0.1..N). */
    setIp(QString("10.0.0.%0").arg(100 + num));
    setMac(QString("00:00:00:00:0d:%0").arg(num, 2, 10, QChar('0')));
}

void DockerNode::draw(NetworkMapDrawer *drawer)
{
    /* Legacy QPixmap-drawer (старая канва). Новая канва (NodeItem) рендерит
     * SVG через ThemeManager. Тут — fallback на host.png если иконка docker
     * не подгружена. */
    QPoint position = getPosition();
    QString name = getName();
    QSize size = getSize();
    int textVerticalOffset = 5;
    QPixmap icon(":images/docker.png");
    if (icon.isNull())
        icon = QPixmap(":images/host.png");
    drawer->drawImage(icon, position);
    drawer->drawDeviceName(name, QPoint(position.x(), position.y() +
                                        size.height() / 2 +
                                        drawer->getTextSize(name).height() / 2 +
                                        textVerticalOffset));
    if (isSelected)
    {
        drawer->drawElementFrame(size, position);
    }
}

DeviceType DockerNode::getDeviceType()
{
    return DOCKERNODE;
}

QSize DockerNode::getSize() const
{
    QPixmap icon(":images/docker.png");
    if (icon.isNull())
        icon = QPixmap(":images/host.png");
    return icon.size();
}
