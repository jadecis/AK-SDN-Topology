#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>
#include <QActionGroup>
#include <QLabel>

namespace Ui
{
class MainWindow;
}

class QDockWidget;
class QTextBrowser;
class QToolBar;
class QAction;
class QMenu;
class QComboBox;
class TopologyView;
class TopologyScene;
class PaletteWidget;
class Core;
class NodeItem;
class LinkItem;
class DockerNode;

/* MainWindow — современный интерфейс приложения:
 *   • Палитра инструментов слева (Select/Link/Delete/Host/Switch/Controller).
 *   • Полное меню (Файл / Правка / Вид / Топология / Алгоритмы / Справка).
 *   • Холст QGraphicsView (TopologyView): wheel-zoom, pan через Space+ЛКМ.
 *   • Журнал в QDockWidget снизу.
 *   • Статусбар: счётчики элементов + статус Mininet + статус Ping.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    /* Привязать Core: создать TopologyScene, привязать к Core::getMap(). */
    void attachCore(Core *core);

    TopologyView *topologyView() const { return topoView_; }
    TopologyScene *topologyScene() const { return topoScene_; }

private slots:
    /* Project / file */
    void onActionNewProject();
    void onActionOpenProject();
    void onActionSave();
    void onActionSaveAs();
    void onActionExportMininet();
    void onActionExportMetrics();
    void onActionExit();

    /* View */
    void onActionToggleLog(bool checked);
    void onActionZoomIn();
    void onActionZoomOut();
    void onActionZoomReset();
    void onActionZoomFit();
    void onActionToggleTheme();
    void onActionMetricDelay();
    void onActionMetricBandwidth();
    void onActionMetricLoss();
    void onActionTogglePalette(bool checked);

    /* Topology actions */
    void onActionStartTopology();
    void onActionStopTopology();
    void onActionRestartTopology();
    void onActionPingAll();
    void onActionMnCleanup();
    void onActionRunRyu();
    void onActionRunMininetOnly();
    void onActionOpenControllerCode();

    /* Edit / layout */
    void onActionAlignH();
    void onActionAlignV();
    void onActionClearMarks();
    void onActionDeleteSelected();

    /* Algorithms */
    void onActionAlgorithms();
    void onActionCreateUserAlgorithm();
    void onActionOpenUserAlgorithmsFolder();
    void onActionOpenControllerWebUI();

    /* Help */
    void onActionAbout();
    void onActionQuickStart();

    void onThemeChanged();

    /* Palette → scene tool change. */
    void onPaletteToolSelected(QString id);

public slots:
    void refreshNetworkMapView(QPixmap);  // ignored: scene rebuilds from model
    void showMessage(QString message);
    void updateStats(int hosts, int switches, int links, int controllers);
    void updatePingStats(int success, int total);
    void setMininetStatus(const QString &status);
    void onProjectChanged(QString dir, QString name);
    void onHostContextRequested(QString hostName, QStringList others, QPoint globalPos);
    void onControllerContextRequested(QPoint globalPos);
    /* From TopologyScene */
    void onSceneNodeContextMenu(NodeItem *n, QPoint globalPos);
    void onSceneLinkContextMenu(LinkItem *li, QPoint globalPos);
    void onSceneEmptyContextMenu(QPoint globalPos);
    void onSceneMessage(QString msg);
    void onSceneModelChanged();

signals:
    /* Старые signals — оставлены для совместимости с Core */
    void signalConnectSdnController();
    void signalClearNetworkMap();

    void signalHandleMouseReleaseEvent(QPoint);
    void signalHandleMousePressEvent(QPoint);
    void signalHandleMouseMoveEvent(QPoint);
    void signalHandleDoubleClickEvent(QPoint);
    void signalHandleKeyDeletePressEvent();

    void signalChangeStateToEdit();
    void signalPrepareSdnController();
    void signalPrepareHost();
    void signalPrepareSwitch();
    void signalPrepareLink();
    void signalPrepareTextLabel();

    void signalShowPorts(bool);
    void signalShowBandwidth();
    void signalShowDelay();
    void signalShowPacketLossRate();

    void signalCreateMininetScript(QString);
    void signalCreateWeightMatrix(QString);

    void signalSaveNetworkMap(QString);
    void signalLoadNetworkMap(QString);

    void signalAlignVertically();
    void signalAlignHorizontally();
    void signalRemoveMarks();

    /* Project + topology lifecycle */
    void signalNewProject(QString projectDir, QString name, int tmplInt);
    void signalOpenProject(QString projectDir);
    void signalSaveProject();
    void signalCloseProject();

    void signalStartTopology();
    void signalStopTopology();
    void signalRunRyu();
    void signalRunMininet();
    void signalMnCleanup();
    void signalPingAll();
    void signalOpenControllerCode();
    void signalOpenHostXterm(QString hostName);

    void signalRightClickOnCanvas(QPoint localPos, QPoint globalPos);
    void signalRunAlgorithm(QString algorithmId, int rootIndex, QString metric);
    void signalOpenAlgorithmsDialog();
    void signalCreateUserAlgorithm(QString fileName, QString className, QString algoName);
    void signalOpenUserAlgorithmsFolder();
    void signalOpenControllerWebUI();

    /* Активный алгоритм/метрика — управление из тулбара. */
    void signalSetRoutingAlgorithm(QString algoId);
    void signalSetRoutingMetric(QString metric);

private:
    Ui::MainWindow *ui;

    /* === Actions === */
    /* Project */
    QAction *actNewProject;
    QAction *actOpenProject;
    QAction *actSave;
    QAction *actSaveAs;
    QAction *actExportMn;
    QAction *actExportMetrics;
    QAction *actExit;

    /* Topology */
    QAction *actStart;
    QAction *actStop;
    QAction *actRestart;
    QAction *actPingAll;
    QAction *actMnCleanup;
    QAction *actRunRyu;
    QAction *actRunMininet;
    QAction *actOpenControllerCode;

    /* View */
    QAction *actToggleLog;
    QAction *actTogglePalette;
    QAction *actZoomIn;
    QAction *actZoomOut;
    QAction *actZoomReset;
    QAction *actZoomFit;
    QAction *actToggleTheme;
    QActionGroup *metricGroup;
    QAction *actMetricDelay;
    QAction *actMetricBw;
    QAction *actMetricLoss;
    QAction *actShowPorts;

    /* Edit / layout */
    QAction *actUndo = nullptr;
    QAction *actRedo = nullptr;
    QAction *actAlignH;
    QAction *actAlignV;
    QAction *actClearMarks;
    QAction *actDeleteSelected;

    /* Algorithms */
    QAction *actAlgorithms;
    QAction *actCreateUserAlgo;
    QAction *actOpenAlgoFolder;
    QAction *actOpenWebUI;

    /* Help */
    QAction *actAbout;
    QAction *actQuickStart;

    /* Toolbar */
    QToolBar *mainToolBar;

    /* Status bar */
    QLabel *statsProjectLbl;
    QLabel *statsHostsLbl;
    QLabel *statsSwitchesLbl;
    QLabel *statsLinksLbl;
    QLabel *statsControllerLbl;
    QLabel *statsMininetLbl;
    QLabel *statsPingLbl;
    QLabel *statsZoomLbl;

    /* Dock + terminal */
    QDockWidget *terminalDock;
    QTextBrowser *terminal;

    /* Canvas */
    TopologyView *topoView_;
    TopologyScene *topoScene_;
    QDockWidget *paletteDock_;
    PaletteWidget *palette_;
    Core *core_;

    QString openedProjectDir;
    QString openedProjectName;

    /* === Setup helpers === */
    void createActions();
    void createMenus();
    void createMainToolBar();
    void createTerminalDock();
    void createStatusBar();
    void createCanvas();
    void createPaletteDock();
    void refreshIcons();
    void applyZoomLabel();

    void setProjectTitle(const QString &name, const QString &dir);
};

#endif // MAINWINDOW_H
