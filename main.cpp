#include "MainWindow.h"
#include <QApplication>
#include "Core.h"
#include "CommandReceiver.h"
#include "ThemeManager.h"
#include "AlgorithmDialog.h"
#include "WelcomeDialog.h"
#include "NewProjectDialog.h"
#include "ProjectManager.h"
#include "MininetHttpClient.h"
#include "TopologyScene.h"
#include "TrafficMonitor.h"
#include "DockerHealthMonitor.h"
#include "NetworkMap.h"
#include "DockerNode.h"

#include <QSettings>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName("SNet");
    QApplication::setApplicationName("Editor");
    QApplication::setApplicationDisplayName("SDN Topology");

    ThemeManager::instance()->loadPersistedTheme();

    MainWindow view;
    Core c;
    view.attachCore(&c);
    CommandReceiver receiver;

    /* === Existing connections === */
    // States connection
    QObject::connect(&view, SIGNAL(signalChangeStateToEdit()), &c, SLOT(changeStateToEdit()));
    QObject::connect(&view, SIGNAL(signalPrepareHost()), &c, SLOT(prepareHost()));
    QObject::connect(&view, SIGNAL(signalPrepareLink()), &c, SLOT(prepareLink()));
    QObject::connect(&view, SIGNAL(signalPrepareSdnController()), &c, SLOT(prepareSdnController()));
    QObject::connect(&view, SIGNAL(signalPrepareSwitch()), &c, SLOT(prepareSwitch()));
    QObject::connect(&view, SIGNAL(signalPrepareTextLabel()), &c, SLOT(prepareTextLabel()));

    // Key/mouse events
    QObject::connect(&view, SIGNAL(signalHandleDoubleClickEvent(QPoint)), &c, SLOT(handleDoubleClickEvent(QPoint)));
    QObject::connect(&view, SIGNAL(signalHandleKeyDeletePressEvent()), &c, SLOT(handleKeyDeletePressEvent()));
    QObject::connect(&view, SIGNAL(signalHandleMouseMoveEvent(QPoint)), &c, SLOT(handleMouseMoveEvent(QPoint)));
    QObject::connect(&view, SIGNAL(signalHandleMousePressEvent(QPoint)), &c, SLOT(handleMousePressEvent(QPoint)));
    QObject::connect(&view, SIGNAL(signalHandleMouseReleaseEvent(QPoint)), &c, SLOT(handleMouseReleaseEvent(QPoint)));

    // Network map actions
    QObject::connect(&view, SIGNAL(signalSaveNetworkMap(QString)), &c, SLOT(saveNetworkMap(QString)));
    QObject::connect(&view, SIGNAL(signalLoadNetworkMap(QString)), &c, SLOT(loadNetworkMap(QString)));
    QObject::connect(&view, SIGNAL(signalCreateMininetScript(QString)), &c, SLOT(createMininetScript(QString)));
    QObject::connect(&view, SIGNAL(signalConnectSdnController()), &c, SLOT(connectSdnController()));
    QObject::connect(&view, SIGNAL(signalClearNetworkMap()), &c, SLOT(clearNetworkMap()));
    QObject::connect(&view, SIGNAL(signalCreateWeightMatrix(QString)), &c, SLOT(createWeightsMatrix(QString)));
    QObject::connect(&view, SIGNAL(signalAlignHorizontally()), &c, SLOT(AlignHorizontally()));
    QObject::connect(&view, SIGNAL(signalAlignVertically()), &c, SLOT(AlignVertically()));

    // Display properties
    QObject::connect(&view, SIGNAL(signalShowBandwidth()), &c, SLOT(showBandwidth()));
    QObject::connect(&view, SIGNAL(signalShowDelay()), &c, SLOT(showDelay()));
    QObject::connect(&view, SIGNAL(signalShowPacketLossRate()), &c, SLOT(showPacketLossRate()));
    QObject::connect(&view, SIGNAL(signalShowPorts(bool)), &c, SLOT(showPorts(bool)));

    // Drawing & stats
    QObject::connect(&c, SIGNAL(signalRefreshNetworkMapView(QPixmap)), &view, SLOT(refreshNetworkMapView(QPixmap)));
    QObject::connect(&c, &Core::signalStatsChanged, &view, &MainWindow::updateStats);
    QObject::connect(&c, &Core::signalMessage, &view, &MainWindow::showMessage);
    QObject::connect(&c, &Core::signalMininetStatusChanged, &view, &MainWindow::setMininetStatus);
    QObject::connect(&c, &Core::signalPingStatsChanged, &view, &MainWindow::updatePingStats);
    QObject::connect(&c, &Core::signalProjectChanged, &view, &MainWindow::onProjectChanged);

    // Network command handler
    QObject::connect(&view, SIGNAL(signalRemoveMarks()), &c, SLOT(eraseMarks()));
    QObject::connect(&receiver, SIGNAL(signalPathReceived(QList<int>)), &c, SLOT(visualizePath(QList<int>)));
    QObject::connect(&receiver, SIGNAL(signalTreeReceived(QList<QList<int> >)), &c, SLOT(visualizeTree(QList<QList<int> >)));
    QObject::connect(&receiver, SIGNAL(signalTreesReceived(QList<QList<QList<int> > >)), &c, SLOT(visualizeTrees(QList<QList<QList<int> > >)));
    QObject::connect(&receiver, SIGNAL(signalIslandsReceived(QList<QList<int> >)), &c, SLOT(visualizeIslands(QList<QList<int> >)));
    QObject::connect(&receiver, SIGNAL(signalPathsReceived(QList<QList<int> >)), &c, SLOT(visualizePaths(QList<QList<int> >)));
    QObject::connect(&receiver, SIGNAL(signalMessageReceived(QString)), &view, SLOT(showMessage(QString)));

    /* === Project workflow === */
    QObject::connect(&view, &MainWindow::signalNewProject, &c, &Core::newProjectFromTemplate);
    QObject::connect(&view, &MainWindow::signalOpenProject, &c, &Core::openProjectFromDir);
    QObject::connect(&view, &MainWindow::signalSaveProject, &c, &Core::saveProjectAll);
    QObject::connect(&view, &MainWindow::signalCloseProject, &c, &Core::closeCurrentProject);

    /* === Topology lifecycle === */
    QObject::connect(&view, &MainWindow::signalStartTopology, &c, &Core::startTopology);
    QObject::connect(&view, &MainWindow::signalStopTopology, &c, &Core::stopTopology);
    QObject::connect(&view, &MainWindow::signalRunMininet, &c, &Core::launchMininet);
    QObject::connect(&view, &MainWindow::signalRunRyu, &c, &Core::launchRyu);
    QObject::connect(&view, &MainWindow::signalPingAll, &c, &Core::pingAll);
    QObject::connect(&view, &MainWindow::signalOpenControllerCode, &c, &Core::openControllerCode);
    QObject::connect(&view, &MainWindow::signalOpenHostXterm, &c, &Core::openHostXterm);
    QObject::connect(&view, &MainWindow::signalMnCleanup, &c, &Core::stopTopology);
    QObject::connect(&view, &MainWindow::signalOpenControllerWebUI,
                     &c, &Core::openControllerWebUI);
    QObject::connect(&view, &MainWindow::signalCreateUserAlgorithm,
                     &c, &Core::createUserAlgorithm);
    QObject::connect(&view, &MainWindow::signalOpenUserAlgorithmsFolder,
                     &c, &Core::openUserAlgorithmsFolder);

    /* === Active routing algorithm + metric === */
    QObject::connect(&view, &MainWindow::signalSetRoutingAlgorithm,
                     &c, &Core::setCurrentRoutingAlgorithm);
    QObject::connect(&view, &MainWindow::signalSetRoutingMetric,
                     &c, &Core::setCurrentRoutingMetric);

    /* === Algorithms === */
    QObject::connect(&view, &MainWindow::signalOpenAlgorithmsDialog, [&]() {
        AlgorithmDialog dlg(c.getMap(), &view);
        /* Используем расширенный сигнал с QVariantMap — он несёт K, λ,
         * ограничения и т.п. Базовая версия signalRun остаётся как
         * fallback (Core::runRoutingAlgorithm проксирует на Ext). */
        QObject::connect(&dlg, &AlgorithmDialog::signalRunWithParams,
                         &c, &Core::runRoutingAlgorithmExt);
        dlg.exec();
    });
    QObject::connect(&c, &Core::signalAlgorithmFinished, &view,
        [&view](QString name, qint64 elapsedMs) {
            view.showMessage(QString("Алгоритм '%1' завершён за %2 мс").arg(name).arg(elapsedMs));
        });

    /* === Context menus on canvas === */
    QObject::connect(&view, &MainWindow::signalRightClickOnCanvas,
                     &c, &Core::handleRightClickEvent);
    QObject::connect(&c, &Core::signalHostContextRequested,
                     &view, &MainWindow::onHostContextRequested);
    QObject::connect(&c, &Core::signalControllerContextRequested,
                     &view, &MainWindow::onControllerContextRequested);

    /* === Live-трафик мониторинг (tcpdump → /traffic_events → animation) === */
    TrafficMonitor trafficMonitor(c.getHttpClient());
    QObject::connect(&trafficMonitor, &TrafficMonitor::packetSeen,
        [&](QString from, QString to, QColor color) {
        if (view.topologyScene())
            view.topologyScene()->animatePacketBetweenHosts(from, to, color);
    });
    /* === Health Docker-узлов (GET /docker/health → индикатор на канве) === */
    DockerHealthMonitor dockerHealthMonitor(c.getHttpClient());
    QObject::connect(&dockerHealthMonitor, &DockerHealthMonitor::healthUpdated,
        [&](QVariantList containers) {
        NetworkMap *map = c.getMap();
        if (!map) return;
        bool changed = false;
        for (const QVariant &v : containers)
        {
            QVariantMap m = v.toMap();
            QString name = m.value("name").toString();
            Node *n = map->getNodeByName(name);
            DockerNode *dn = dynamic_cast<DockerNode *>(n);
            if (!dn) continue;
            QString status = m.value("status").toString();
            QString health = m.value("health").toString();
            bool healthy = m.value("healthy").toBool();
            DockerNode::RuntimeStatus rs;
            if (status != "running")        rs = DockerNode::StatusDead;
            else if (health == "starting")  rs = DockerNode::StatusStarting;
            else if (healthy)               rs = DockerNode::StatusHealthy;
            else                            rs = DockerNode::StatusDead;
            if (dn->getRuntimeStatus() != rs) { dn->setRuntimeStatus(rs); changed = true; }
        }
        if (changed && view.topologyScene())
            view.topologyScene()->update();
    });

    /* Запускаем мониторинг по факту старта Mininet и останавливаем по стопу. */
    QObject::connect(&c, &Core::signalMininetStatusChanged, [&](QString status) {
        if (status == QObject::tr("работает") || status == QObject::tr("запускается"))
        {
            if (!trafficMonitor.isRunning()) trafficMonitor.start(500);
            if (!dockerHealthMonitor.isRunning()) dockerHealthMonitor.start(3000);
        }
        else
        {
            trafficMonitor.stop();
            dockerHealthMonitor.stop();
            /* Сбрасываем индикаторы в «unknown» при остановке. */
            if (c.getMap())
            {
                for (DockerNode *dn : c.getMap()->getDockerNodes())
                    dn->setRuntimeStatus(DockerNode::StatusUnknown);
                if (view.topologyScene()) view.topologyScene()->update();
            }
        }
    });

    /* === Route highlight + packet animation on new canvas === */
    QObject::connect(&c, &Core::signalHighlightTree, [&](QList<QList<int>> tree, QColor color) {
        if (view.topologyScene())
            view.topologyScene()->highlightTreeByGroupId(tree, color);
    });
    QObject::connect(&c, &Core::signalHighlightPath, [&](QList<int> path, QColor color) {
        if (view.topologyScene())
            view.topologyScene()->highlightPathByGroupId(path, color);
    });
    QObject::connect(&c, &Core::signalClearHighlights, [&]() {
        if (view.topologyScene())
            view.topologyScene()->clearAllHighlights();
    });
    QObject::connect(&c, &Core::signalHighlightSegments,
        [&](QList<QList<int>> segments) {
        if (view.topologyScene())
            view.topologyScene()->highlightSegmentsByGroupId(segments);
    });
    QObject::connect(&c, &Core::signalHighlightTrees,
        [&](QList<QList<QList<int>>> trees, QList<QColor> colors) {
        if (view.topologyScene())
            view.topologyScene()->highlightTreesByGroupId(trees, colors);
    });
    QObject::connect(&c, &Core::signalAnimatePacket,
        [&](QString from, QString to, QColor color) {
        if (view.topologyScene())
            view.topologyScene()->animatePacketBetweenHosts(from, to, color);
    });

    view.show();
    receiver.on();

    /* === Welcome dialog при старте (только если не отключено) === */
    QSettings settings("SNet", "Editor");
    bool skipWelcome = settings.value("ui/skip_welcome", false).toBool();
    if (!skipWelcome)
    {
        WelcomeDialog welcome(&view);
        welcome.exec();

        WelcomeDialog::Action act = welcome.chosenAction();
        if (act == WelcomeDialog::ActionNewProject)
        {
            NewProjectDialog dlg(&view);
            if (dlg.exec() == QDialog::Accepted)
            {
                c.newProjectFromTemplate(dlg.fullProjectDir(),
                                         dlg.projectName(),
                                         int(dlg.chosenTemplate()));
            }
        }
        else if (act == WelcomeDialog::ActionOpenProject)
        {
            QString dir = welcome.chosenProjectDir();
            if (!dir.isEmpty() && QFileInfo::exists(dir))
            {
                c.openProjectFromDir(dir);
            }
        }
    }

    return a.exec();
}
