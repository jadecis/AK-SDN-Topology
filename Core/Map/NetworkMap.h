#ifndef NETWORKMAP_H
#define NETWORKMAP_H

#include <QList>
#include "SelectionFrame.h"
#include "MatrixRepository.h"

class QPixmap;
class Graph;
class Link;
class Node;
class Element;
class SSLink;
class CSLink;
class Switch;
class Host;
class SdnController;
class TextLabel;
class DockerNode;

class NetworkMap
{
public:
    ~NetworkMap();
    void clear();
    void removeElement(Element *e);


    Node *getNodeByPosition(QPoint position);
    Node *getNodeByName(QString name);
    Element *getElementByPosition(QPoint position);
    QList<Node *> getNodesBySelectedFrame();
    inline QList<Switch *> getSwitches() const;
    inline QList<Host *> getHosts() const;
    inline QList<DockerNode *> getDockerNodes() const;
    inline QList<SdnController *> getSdnControllers() const;
    inline QList<TextLabel *> getTextLabels() const { return textLabels; }
    inline QList<SSLink *> getSSLinks() const;
    inline QList<CSLink *> getCSLinks() const;
    inline int getHostCount() const;
    inline int getSwitchCount() const;
    inline int getLinkCount() const;
    inline int getSdnControllerCount() const;
    inline CommutationMatrix<SSLink> getGraphMatrix();
    inline SelectionFrame *getSelectionFrame();
    /* Снимок текущей PortMatrix — для отображения номеров портов в UI. */
    inline PortMatrix getPortMatrix() const { return matrices.getPortMatrix(); }

    inline bool isConnected(Node *node1, Node *node2);

    void addSSLink(SSLink *ln);
    void addCSLink(CSLink *ln);
    void addSwitch(Switch *sw);
    void addHost(Host *hst);
    void addDockerNode(DockerNode *d);
    void addSdnController(SdnController *c);
    void addTextLabel(TextLabel *txt);

    void removeSSLink(SSLink *ln);
    void removeCSLink(CSLink *ln);
    void removeSwitch(Switch *sw);
    void removeHost(Host *hst);
    void removeDockerNode(DockerNode *d);
    void removeSdnController(SdnController *c);
    void removeTextLabel(TextLabel *txt);

    void connectSdnController();   

    void visualizePath(QList<int> path);
    void visualizePaths(QList<QList<int> > paths);
    void visualizeTree(QList<QList<int> > tree);
    void visualizeTrees(QList<QList<QList<int> > > trees);
    void visualizeIslands(QList<QList<int>> islands);
    void unselectLinks();
    void unselectNodes();

    void changeMetric(QVector<float> metricData);

    QString createMininetScript();
    QString createXmlDocument();
    QPixmap draw();

private:
    QList<Node *> nodes;
    QList<Link *> links;
    QList<SSLink *> ssLinks;
    QList<CSLink *> csLinks;
    QList<Switch *> switches;
    QList<Host *> hosts;
    QList<DockerNode *> dockerNodes;
    QList<SdnController *> sdnControllers;
    QList<TextLabel *> textLabels;

    SelectionFrame selectionFrame;
    MatrixRepository matrices;
    int globalId;

private:
    void disconnectNode(Node *node);
};

inline QList<Switch *> NetworkMap::getSwitches() const
{
    return switches;
}

inline QList<Host *> NetworkMap::getHosts() const
{
    return hosts;
}

inline QList<DockerNode *> NetworkMap::getDockerNodes() const
{
    return dockerNodes;
}

inline QList<SdnController *> NetworkMap::getSdnControllers() const
{
    return sdnControllers;
}

inline QList<SSLink *> NetworkMap::getSSLinks() const
{
    return ssLinks;
}

inline QList<CSLink *> NetworkMap::getCSLinks() const
{
    return csLinks;
}

inline int NetworkMap::getHostCount() const
{
    return hosts.size();
}

inline int NetworkMap::getSwitchCount() const
{
    return switches.size();
}

inline int NetworkMap::getLinkCount() const
{
    return links.size();
}

inline int NetworkMap::getSdnControllerCount() const
{
    return sdnControllers.size();
}

inline CommutationMatrix<SSLink> NetworkMap::getGraphMatrix()
{
    return matrices.getGraphMatrix();
}

inline bool NetworkMap::isConnected(Node *node1, Node *node2)
{
    return matrices.getGlobalMatrix().getLink(node1, node2) != NULL;
}

inline SelectionFrame *NetworkMap::getSelectionFrame()
{
    return &selectionFrame;
}

#endif // NETWORKMAP_H
