#ifndef XMLBUILDER_H
#define XMLBUILDER_H

#include <QtXml>

class Switch;
class Host;
class DockerNode;
class SdnController;
class Link;
class Node;
class SSLink;
class CSLink;
class TextLabel;

class XmlSerializer
{
public:
    void addSwitchData(Switch *sw);
    void addSdnControllerData(SdnController *controller);
    void addHostData(Host *host);
    void addDockerNodeData(DockerNode *d);
    void addTextLabelData(TextLabel *textLabel);
    void addSSLinkData(SSLink *link);
    void addCSLinkData(CSLink *link);

    QString buildXmlDocument();

private:
    QList<QDomElement> switchesData;
    QList<QDomElement> sdnControllersData;
    QList<QDomElement> hostsData;
    QList<QDomElement> dockerNodesData;
    QList<QDomElement> switchToSwitchLinksData;
    QList<QDomElement> controllerToSwitchLinksData;
    QList<QDomElement> textLabelData;
    QDomDocument xmlDocument;
};

#endif // XMLBUILDER_H
