#include "XmlSerializer.h"
#include "Node.h"
#include "Switch.h"
#include "Host.h"
#include "DockerNode.h"
#include "SdnController.h"
#include "SSLink.h"
#include "CSLink.h"
#include "TextLabel.h"

void XmlSerializer::addSwitchData(Switch *sw)
{
    QDomElement switchXml = xmlDocument.createElement("Switch");
    QDomAttr xXml = xmlDocument.createAttribute("x");
    xXml.setValue(QString::number(sw->getPosition().x()));
    switchXml.setAttributeNode(xXml);
    QDomAttr yXml = xmlDocument.createAttribute("y");
    yXml.setValue(QString::number(sw->getPosition().y()));
    switchXml.setAttributeNode(yXml);
    switchesData.append(switchXml);
}

void XmlSerializer::addSdnControllerData(SdnController *controller)
{
    QDomElement sdnControllerXml = xmlDocument.createElement("Controller");
    QDomAttr xXml = xmlDocument.createAttribute("x");
    xXml.setValue(QString::number(controller->getPosition().x()));
    sdnControllerXml.setAttributeNode(xXml);
    QDomAttr yXml = xmlDocument.createAttribute("y");
    yXml.setValue(QString::number(controller->getPosition().y()));
    sdnControllerXml.setAttributeNode(yXml);
    QDomAttr portXml = xmlDocument.createAttribute("port");
    portXml.setValue(controller->getPort());
    sdnControllerXml.setAttributeNode(portXml);
    QDomAttr ipXml = xmlDocument.createAttribute("ip");
    ipXml.setValue(controller->getIp());
    sdnControllerXml.setAttributeNode(ipXml);
    sdnControllersData.append(sdnControllerXml);
}

void XmlSerializer::addHostData(Host *host)
{
    QDomElement hostXml = xmlDocument.createElement("Host");
    QDomAttr xXml = xmlDocument.createAttribute("x");
    xXml.setValue(QString::number(host->getPosition().x()));
    hostXml.setAttributeNode(xXml);
    QDomAttr yXml = xmlDocument.createAttribute("y");
    yXml.setValue(QString::number(host->getPosition().y()));
    hostXml.setAttributeNode(yXml);
    QDomAttr macXml = xmlDocument.createAttribute("mac");
    macXml.setValue(host->getMac());
    hostXml.setAttributeNode(macXml);
    QDomAttr ipXml = xmlDocument.createAttribute("ip");
    ipXml.setValue(host->getIp());
    hostXml.setAttributeNode(ipXml);
    hostsData.append(hostXml);
}

void XmlSerializer::addTextLabelData(TextLabel *textLabel)
{
    QDomElement textLabelXml = xmlDocument.createElement("Text");
    QDomAttr contentXml = xmlDocument.createAttribute("content");
    contentXml.setValue(textLabel->getContent());
    textLabelXml.setAttributeNode(contentXml);
    QDomAttr xXml = xmlDocument.createAttribute("x");
    xXml.setValue(QString::number(textLabel->getPosition().x()));
    textLabelXml.setAttributeNode(xXml);
    QDomAttr yXml = xmlDocument.createAttribute("y");
    yXml.setValue(QString::number(textLabel->getPosition().y()));
    textLabelXml.setAttributeNode(yXml);
    textLabelData.append(textLabelXml);
}

void XmlSerializer::addSSLinkData(SSLink *link)
{
    QDomElement linkXml = xmlDocument.createElement("Link");
    QDomAttr node1Xml = xmlDocument.createAttribute("node1");
    node1Xml.setValue(link->getNode1()->getName());
    linkXml.setAttributeNode(node1Xml);
    QDomAttr node2Xml = xmlDocument.createAttribute("node2");
    node2Xml.setValue(link->getNode2()->getName());
    linkXml.setAttributeNode(node2Xml);
    QDomAttr delayXml = xmlDocument.createAttribute("delay");
    delayXml.setValue(QString::number(link->getDelay()));
    linkXml.setAttributeNode(delayXml);
    QDomAttr bandwidthXml = xmlDocument.createAttribute("bandwidth");
    bandwidthXml.setValue(QString::number(link->getBandwidth()));
    linkXml.setAttributeNode(bandwidthXml);
    QDomAttr packetLossXml = xmlDocument.createAttribute("loss");
    packetLossXml.setValue(QString::number(link->getPacketLoss()));
    linkXml.setAttributeNode(packetLossXml);
    QDomAttr disabledXml = xmlDocument.createAttribute("disabled");
    disabledXml.setValue(link->isDisabled() ? "true" : "false");
    linkXml.setAttributeNode(disabledXml);
    switchToSwitchLinksData.append(linkXml);
}

void XmlSerializer::addCSLinkData(CSLink *link)
{
    QDomElement linkXml = xmlDocument.createElement("Link");
    QDomAttr node1Xml = xmlDocument.createAttribute("node1");
    node1Xml.setValue(link->getNode1()->getName());
    linkXml.setAttributeNode(node1Xml);
    QDomAttr node2Xml = xmlDocument.createAttribute("node2");
    node2Xml.setValue(link->getNode2()->getName());
    linkXml.setAttributeNode(node2Xml);
    controllerToSwitchLinksData.append(linkXml);
}

void XmlSerializer::addDockerNodeData(DockerNode *d)
{
    QDomElement el = xmlDocument.createElement("Docker");

    QDomAttr xA = xmlDocument.createAttribute("x");
    xA.setValue(QString::number(d->getPosition().x()));
    el.setAttributeNode(xA);
    QDomAttr yA = xmlDocument.createAttribute("y");
    yA.setValue(QString::number(d->getPosition().y()));
    el.setAttributeNode(yA);

    QDomAttr ipA = xmlDocument.createAttribute("ip");
    ipA.setValue(d->getIp());
    el.setAttributeNode(ipA);
    QDomAttr macA = xmlDocument.createAttribute("mac");
    macA.setValue(d->getMac());
    el.setAttributeNode(macA);
    QDomAttr imageA = xmlDocument.createAttribute("image");
    imageA.setValue(d->getImage());
    el.setAttributeNode(imageA);
    QDomAttr cmdA = xmlDocument.createAttribute("command");
    cmdA.setValue(d->getCommand());
    el.setAttributeNode(cmdA);

    /* env / volumes / ports — как child elements (по одному на строку). */
    for (const QString &v : d->getEnvironment())
    {
        QDomElement c = xmlDocument.createElement("Env");
        c.appendChild(xmlDocument.createTextNode(v));
        el.appendChild(c);
    }
    for (const QString &v : d->getVolumes())
    {
        QDomElement c = xmlDocument.createElement("Volume");
        c.appendChild(xmlDocument.createTextNode(v));
        el.appendChild(c);
    }
    for (const QString &v : d->getPublishedPorts())
    {
        QDomElement c = xmlDocument.createElement("Port");
        c.appendChild(xmlDocument.createTextNode(v));
        el.appendChild(c);
    }

    dockerNodesData.append(el);
}

QString XmlSerializer::buildXmlDocument()
{
    QDomElement networkXml = xmlDocument.createElement("Network");
    QDomElement hostsXml = xmlDocument.createElement("Hosts");
    QDomElement dockersXml = xmlDocument.createElement("Dockers");
    QDomElement switchesXml = xmlDocument.createElement("Switches");
    QDomElement textLabelsXml = xmlDocument.createElement("Texts");
    QDomElement sdnControllersXml = xmlDocument.createElement("Controllers");
    QDomElement switchToSwitchLinksXml = xmlDocument.createElement("SSLinks");
    QDomElement controllerToSwitchLinksXml = xmlDocument.createElement("CSLinks");
    networkXml.appendChild(hostsXml);
    foreach (QDomElement hostXml, hostsData)
    {
        hostsXml.appendChild(hostXml);
    }
    networkXml.appendChild(dockersXml);
    foreach (QDomElement dXml, dockerNodesData)
    {
        dockersXml.appendChild(dXml);
    }
    networkXml.appendChild(switchesXml);
    foreach (QDomElement switchXml, switchesData)
    {
        switchesXml.appendChild(switchXml);
    }
    networkXml.appendChild(sdnControllersXml);
    foreach (QDomElement sdnControllerXml, sdnControllersData)
    {
        sdnControllersXml.appendChild(sdnControllerXml);
    }
    networkXml.appendChild(switchToSwitchLinksXml);
    foreach (QDomElement switchToSwitchLinkXml, switchToSwitchLinksData)
    {
        switchToSwitchLinksXml.appendChild(switchToSwitchLinkXml);
    }
    networkXml.appendChild(controllerToSwitchLinksXml);
    foreach (QDomElement controllerToSwitchLinkXml, controllerToSwitchLinksData)
    {
        controllerToSwitchLinksXml.appendChild(controllerToSwitchLinkXml);
    }
    foreach (QDomElement textLabelXml, textLabelData)
    {
        textLabelsXml.appendChild(textLabelXml);
    }
    networkXml.appendChild(textLabelsXml);
    xmlDocument.appendChild(networkXml);
    return QString("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE topology>\n")
            .append(xmlDocument.toString());
}
