#include "XmlDeserializer.h"
#include "NetworkMap.h"
#include "SSLink.h"
#include "CSLink.h"
#include "Host.h"
#include "DockerNode.h"
#include "Switch.h"
#include "SdnController.h"
#include "TextLabel.h"
#include <QtXml>

void XmlDeserializer::deserialize(QString xmlFile, NetworkMap *map)
{
    map->clear();
    QDomDocument xmlDocument;
    xmlDocument.setContent(xmlFile);
    QDomElement networkXml = xmlDocument.firstChildElement("Network");

    QDomNodeList hostsXml = networkXml.firstChildElement("Hosts").childNodes();
    for (int i = 0; i < hostsXml.length(); i++)
    {
        QDomElement hostXml = hostsXml.at(i).toElement();
        int x = hostXml.attributeNode("x").value().toInt();
        int y = hostXml.attributeNode("y").value().toInt();
        map->addHost(new Host(QPoint(x, y)));
    }

    QDomNodeList dockersXml = networkXml.firstChildElement("Dockers").childNodes();
    for (int i = 0; i < dockersXml.length(); i++)
    {
        QDomElement dXml = dockersXml.at(i).toElement();
        if (dXml.isNull()) continue;
        int x = dXml.attributeNode("x").value().toInt();
        int y = dXml.attributeNode("y").value().toInt();
        DockerNode *d = new DockerNode(QPoint(x, y));
        map->addDockerNode(d);  /* setup() уже выставил дефолты */
        if (dXml.hasAttribute("ip")) d->setIp(dXml.attribute("ip"));
        if (dXml.hasAttribute("mac")) d->setMac(dXml.attribute("mac"));
        if (dXml.hasAttribute("image")) d->setImage(dXml.attribute("image"));
        if (dXml.hasAttribute("command")) d->setCommand(dXml.attribute("command"));
        QStringList env, vols, ports;
        QDomNodeList children = dXml.childNodes();
        for (int j = 0; j < children.length(); j++)
        {
            QDomElement c = children.at(j).toElement();
            if (c.isNull()) continue;
            QString text = c.text();
            if (c.tagName() == "Env") env << text;
            else if (c.tagName() == "Volume") vols << text;
            else if (c.tagName() == "Port") ports << text;
        }
        d->setEnvironment(env);
        d->setVolumes(vols);
        d->setPublishedPorts(ports);
    }

    QDomNodeList textLabelsXml = networkXml.firstChildElement("Texts").childNodes();
    for (int i = 0; i < textLabelsXml.length(); i++)
    {
        QDomElement textLabelXml = textLabelsXml.at(i).toElement();
        int x = textLabelXml.attributeNode("x").value().toInt();
        int y = textLabelXml.attributeNode("y").value().toInt();
        QString content = textLabelXml.attributeNode("content").value();

        TextLabel *txt = new TextLabel(QPoint(x, y));
        txt->setContent(content);
        map->addTextLabel(txt);
    }

    QDomNodeList switchesXml = networkXml.firstChildElement("Switches").childNodes();
    for (int i = 0; i < switchesXml.length(); i++)
    {
        QDomElement switchXml = switchesXml.at(i).toElement();
        int x = switchXml.attributeNode("x").value().toInt();
        int y = switchXml.attributeNode("y").value().toInt();
        map->addSwitch(new Switch(QPoint(x, y)));
    }

    QDomNodeList sdnControllersXml = networkXml.firstChildElement("Controllers").childNodes();
    for (int i = 0; i < sdnControllersXml.length(); i++)
    {
        QDomElement sdnControllerXml = sdnControllersXml.at(i).toElement();
        int x = sdnControllerXml.attributeNode("x").value().toInt();
        int y = sdnControllerXml.attributeNode("y").value().toInt();
        QString ip = sdnControllerXml.attributeNode("ip").value();
        QString port = sdnControllerXml.attributeNode("port").value();
        SdnController *controller = new SdnController(QPoint(x, y));
        map->addSdnController(controller);
        controller->setIp(ip);
        controller->setPort(port);
    }

    QDomNodeList switchToSwitchLinksXml = networkXml.firstChildElement("SSLinks").childNodes();
    for (int i = 0; i < switchToSwitchLinksXml.length(); i++)
    {
        QDomElement linkXml = switchToSwitchLinksXml.at(i).toElement();
        QString node1Name = linkXml.attributeNode("node1").value();
        QString node2Name = linkXml.attributeNode("node2").value();
        float delay = linkXml.attributeNode("delay").value().toFloat();
        float bandwidth = linkXml.attributeNode("bandwidth").value().toFloat();
        float packetLossRate = linkXml.attributeNode("loss").value().toFloat();
        bool disabled = linkXml.attributeNode("disabled").value() == "true";

        Node *node1 = map->getNodeByName(node1Name);
        Node *node2 = map->getNodeByName(node2Name);
        SSLink *ln = new SSLink(node1, node2);
        ln->setDelay(delay);
        ln->setBandwidth(bandwidth);
        ln->setPacketLossRate(packetLossRate);
        ln->setDisabled(disabled);
        map->addSSLink(ln);
    }

    QDomNodeList controllerToSwitchLinksXml = networkXml.firstChildElement("CSLinks").childNodes();
    for (int i = 0; i < controllerToSwitchLinksXml.length(); i++)
    {
        QDomElement linkXml = controllerToSwitchLinksXml.at(i).toElement();
        QString node1Name = linkXml.attributeNode("node1").value();
        QString node2Name = linkXml.attributeNode("node2").value();

        Node *node1 = map->getNodeByName(node1Name);
        Node *node2 = map->getNodeByName(node2Name);
        map->addCSLink(new CSLink(node1, node2));
    }
}
