#ifndef MININETSCRIPTBUILDER_H
#define MININETSCRIPTBUILDER_H

#include <QStringList>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include "PortMatrix.h"

class Switch;
class Host;
class DockerNode;
class SdnController;
class SSLink;

class MininetScriptBuilder
{
public:
    MininetScriptBuilder(PortMatrix portMatrix);
    void addSwitchData(Switch *sw);
    void addSdnControllerData(SdnController *controller);
    void addHostData(Host *host);
    void addDockerNodeData(DockerNode *d);
    void addSSLinkData(SSLink *link);

    /* Если true — генерируется скрипт на основе Containernet
     * (поддерживает addDocker). Иначе — обычный Mininet. */
    void setUseContainernet(bool on) { useContainernet_ = on; }

    QString buildMininetScript();

private:
    QStringList switchesData;
    QStringList sdnControllersData;
    QStringList hostsData;
    QStringList dockerNodesData;
    /* Уникальные имена образов — для пред-pull через subprocess до того,
     * как Containernet попробует свой dcli.pull() (который иногда
     * подвисает на stream-парсинге даже для локальных образов). */
    QSet<QString> dockerImagesSet_;
    /* (имя, ip) для каждого Docker-узла — нужно чтобы после net.build()
     * вручную поднять интерфейс d1-eth0 и назначить IP, потому что
     * Containernet НЕ делает это автоматически. */
    QList<QPair<QString, QString>> dockerSetupData;
    QStringList linksData;
    QStringList startingData;
    bool useContainernet_ = false;

    QString buildIncludeBlock();
    QString buildFunctionBeginBlock();

    QString buildSdnControllersBlock();
    QString buildHostsBlock();
    QString buildDockerNodesBlock();
    QString buildSwitchesBlock();
    QString buildLinksBlock();
    QString buildStartingBlock();

    QString buildFunctionEndBlock();
    QString buildStartupBlock();

private:
    PortMatrix portMatrix;
};

#endif // MININETSCRIPTBUILDER_H
