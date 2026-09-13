#include "MainWindow.h"
#include "ui_mainwindow.h"
#include "ThemeManager.h"
#include "NewProjectDialog.h"
#include "CreateAlgorithmDialog.h"
#include "ProjectManager.h"
#include "TopologyView.h"
#include "TopologyScene.h"
#include "PaletteWidget.h"
#include "NodeItem.h"
#include "LinkItem.h"
#include "Core.h"
#include "Host.h"
#include "SSLink.h"
#include "CSLink.h"
#include "ProcessLauncher.h"
#include "MininetHttpClient.h"
#include "DockerNode.h"
#include <QProcess>
#include "NetworkMap.h"
#include "Link.h"
#include "Node.h"
#include "AlgorithmRegistry.h"
#include "IRoutingAlgorithm.h"

#include <QtWidgets>
#include <QFileDialog>
#include <QTime>
#include <QTextBrowser>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    metricGroup(nullptr),
    mainToolBar(nullptr),
    terminalDock(nullptr),
    terminal(nullptr),
    topoView_(nullptr),
    topoScene_(nullptr),
    paletteDock_(nullptr),
    palette_(nullptr),
    core_(nullptr)
{
    ui->setupUi(this);

    setWindowTitle(tr("SDN Topology — Редактор ПКС"));
    setWindowIcon(ThemeManager::instance()->makeIcon(":/modern/modern/app.svg",
                                                     QSize(48, 48)));
    /* Минимальный размер сделан реально маленьким, чтобы окно влезало на
     * нетбуке и журнал не съедал половину экрана. setMinimumHeight
     * терминала тоже снизили (см. createTerminalDock). resize задаёт
     * комфортный размер при первом старте, но не блокирует уменьшение. */
    setMinimumSize(900, 560);
    resize(1280, 820);

    createActions();
    createMenus();
    createMainToolBar();
    createCanvas();
    createPaletteDock();
    createTerminalDock();
    createStatusBar();
    /* По умолчанию Qt в нижнем-левом углу отдаёт пространство BOTTOM-доку.
     * Это значит, что Журнал отрезает Палитру по высоте — её нижние кнопки
     * (Текст, Docker) уезжают. Передаём угол LEFT-доку — теперь Палитра
     * идёт во всю высоту окна, а Журнал занимает ширину между палитрой и
     * правым краем. */
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);

    /* Связь с ThemeManager. */
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
}

void MainWindow::attachCore(Core *core)
{
    core_ = core;
    if (!core_) return;
    if (topoScene_)
    {
        topoScene_->deleteLater();
        topoScene_ = nullptr;
    }
    topoScene_ = new TopologyScene(core_, this);
    topoView_->setTopologyScene(topoScene_);
    topoScene_->rebuildFromModel();

    connect(topoScene_, &TopologyScene::nodeContextMenu,
            this, &MainWindow::onSceneNodeContextMenu);
    connect(topoScene_, &TopologyScene::linkContextMenu,
            this, &MainWindow::onSceneLinkContextMenu);
    connect(topoScene_, &TopologyScene::emptyContextMenu,
            this, &MainWindow::onSceneEmptyContextMenu);
    connect(topoScene_, &TopologyScene::messageRequested,
            this, &MainWindow::onSceneMessage);
    connect(topoScene_, &TopologyScene::modelChanged,
            this, &MainWindow::onSceneModelChanged);

    /* Undo/Redo (#6): после восстановления модели пересобираем сцену; по
     * изменению доступности — обновляем кнопки/меню. */
    connect(core_, &Core::signalModelRestored, this, [this]() {
        if (topoScene_) topoScene_->rebuildFromModel();
        onSceneModelChanged();
    });
    connect(core_, &Core::signalUndoRedoChanged, this,
            [this](bool canUndo, bool canRedo) {
        if (actUndo) actUndo->setEnabled(canUndo);
        if (actRedo) actRedo->setEnabled(canRedo);
    });
    if (actUndo) actUndo->setEnabled(core_->canUndo());
    if (actRedo) actRedo->setEnabled(core_->canRedo());

    if (palette_)
    {
        connect(palette_, &PaletteWidget::toolSelected,
                this, &MainWindow::onPaletteToolSelected,
                Qt::UniqueConnection);
        topoScene_->setActiveTool(palette_->activeTool());
    }
}

void MainWindow::createCanvas()
{
    topoView_ = new TopologyView(this);
    QVBoxLayout *lay = qobject_cast<QVBoxLayout *>(ui->canvasHost->layout());
    if (lay) lay->addWidget(topoView_);

    connect(topoView_, &TopologyView::zoomChanged, this, [this](double z) {
        if (statsZoomLbl) statsZoomLbl->setText(QString("%1%").arg(qRound(z * 100)));
    });
}

void MainWindow::createPaletteDock()
{
    palette_ = new PaletteWidget(this);
    paletteDock_ = new QDockWidget(tr("Палитра"), this);
    paletteDock_->setObjectName("paletteDock");
    paletteDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    paletteDock_->setFeatures(QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetFloatable |
                              QDockWidget::DockWidgetClosable);
    paletteDock_->setWidget(palette_);
    addDockWidget(Qt::LeftDockWidgetArea, paletteDock_);

    /* Синхронизируем чекбокс actTogglePalette при закрытии дока. */
    if (actTogglePalette)
    {
        connect(paletteDock_, &QDockWidget::visibilityChanged,
                actTogglePalette, &QAction::setChecked);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::createActions()
{
    ThemeManager *tm = ThemeManager::instance();

    /* ===== Project / file ===== */
    actNewProject = new QAction(tm->makeIcon(":/modern/modern/new.svg"),
                                tr("Новый проект…"), this);
    actNewProject->setShortcut(QKeySequence::New);

    actOpenProject = new QAction(tm->makeIcon(":/modern/modern/open.svg"),
                                 tr("Открыть проект…"), this);
    actOpenProject->setShortcut(QKeySequence::Open);

    actSave = new QAction(tm->makeIcon(":/modern/modern/save.svg"),
                          tr("Сохранить"), this);
    actSave->setShortcut(QKeySequence::Save);

    actSaveAs = new QAction(tm->makeIcon(":/modern/modern/save_as.svg"),
                            tr("Экспортировать topology.sdn.xml…"), this);

    actExportMn = new QAction(tm->makeIcon(":/modern/modern/python.svg"),
                              tr("Экспортировать Mininet-скрипт…"), this);

    actExportMetrics = new QAction(tr("Экспортировать матрицу метрик…"), this);

    actExit = new QAction(tr("Выход"), this);
    actExit->setShortcut(QKeySequence::Quit);

    connect(actNewProject, &QAction::triggered, this, &MainWindow::onActionNewProject);
    connect(actOpenProject, &QAction::triggered, this, &MainWindow::onActionOpenProject);
    connect(actSave, &QAction::triggered, this, &MainWindow::onActionSave);
    connect(actSaveAs, &QAction::triggered, this, &MainWindow::onActionSaveAs);
    connect(actExportMn, &QAction::triggered, this, &MainWindow::onActionExportMininet);
    connect(actExportMetrics, &QAction::triggered, this, &MainWindow::onActionExportMetrics);
    connect(actExit, &QAction::triggered, this, &MainWindow::onActionExit);

    /* ===== Topology lifecycle ===== */
    actStart = new QAction(tm->makeIcon(":/modern/modern/run_mininet.svg",
                                        tm->successColor()),
                           tr("Запустить топологию"), this);
    actStart->setShortcut(QKeySequence("F5"));
    actStart->setStatusTip(tr("Запустить Mininet и Ryu (F5)"));

    actStop = new QAction(tm->makeIcon(":/modern/modern/clear_marks.svg",
                                       tm->dangerColor()),
                          tr("Остановить топологию"), this);
    actStop->setShortcut(QKeySequence("Shift+F5"));
    actStop->setStatusTip(tr("Остановить Mininet (Shift+F5)"));

    actRestart = new QAction(tr("Перезапустить топологию"), this);
    actRestart->setShortcut(QKeySequence("Ctrl+F5"));

    actPingAll = new QAction(tm->makeIcon(":/modern/modern/ping.svg",
                                          tm->successColor()),
                             tr("Ping All"), this);
    actPingAll->setShortcut(QKeySequence("Ctrl+P"));
    actPingAll->setStatusTip(tr("Запустить pingAll и подсветить связи (Ctrl+P)"));

    actMnCleanup = new QAction(tr("Очистить mininet (sudo mn -c)"), this);

    actRunRyu = new QAction(tm->makeIcon(":/modern/modern/run_ryu.svg"),
                            tr("Запустить только Ryu"), this);
    actRunMininet = new QAction(tm->makeIcon(":/modern/modern/run_mininet.svg"),
                                tr("Запустить только Mininet"), this);
    actOpenControllerCode = new QAction(tm->makeIcon(":/modern/modern/edit.svg"),
                                        tr("Открыть код контроллера"), this);

    connect(actStart, &QAction::triggered, this, &MainWindow::onActionStartTopology);
    connect(actStop, &QAction::triggered, this, &MainWindow::onActionStopTopology);
    connect(actRestart, &QAction::triggered, this, &MainWindow::onActionRestartTopology);
    connect(actPingAll, &QAction::triggered, this, &MainWindow::onActionPingAll);
    connect(actMnCleanup, &QAction::triggered, this, &MainWindow::onActionMnCleanup);
    connect(actRunRyu, &QAction::triggered, this, &MainWindow::onActionRunRyu);
    connect(actRunMininet, &QAction::triggered, this, &MainWindow::onActionRunMininetOnly);
    connect(actOpenControllerCode, &QAction::triggered, this, &MainWindow::onActionOpenControllerCode);

    /* ===== View ===== */
    actToggleLog = new QAction(tr("Журнал"), this);
    actToggleLog->setShortcut(QKeySequence("Ctrl+L"));
    actToggleLog->setCheckable(true);
    actToggleLog->setChecked(true);

    actTogglePalette = new QAction(tr("Палитра инструментов"), this);
    actTogglePalette->setShortcut(QKeySequence("Ctrl+B"));
    actTogglePalette->setCheckable(true);
    actTogglePalette->setChecked(true);

    actZoomIn = new QAction(tr("Увеличить"), this);
    actZoomIn->setShortcut(QKeySequence("Ctrl+="));
    actZoomOut = new QAction(tr("Уменьшить"), this);
    actZoomOut->setShortcut(QKeySequence("Ctrl+-"));
    actZoomReset = new QAction(tr("Масштаб 100%"), this);
    actZoomReset->setShortcut(QKeySequence("Ctrl+0"));
    actZoomFit = new QAction(tr("Подогнать по окну"), this);

    actToggleTheme = new QAction(tm->makeIcon(":/modern/modern/theme.svg"),
                                 tr("Переключить тему"), this);
    actToggleTheme->setShortcut(QKeySequence("Ctrl+T"));

    metricGroup = new QActionGroup(this);
    actMetricDelay = new QAction(tr("Задержка (delay)"), this);
    actMetricBw = new QAction(tr("Полоса (bandwidth)"), this);
    actMetricLoss = new QAction(tr("Потери (loss)"), this);
    actMetricDelay->setCheckable(true);
    actMetricBw->setCheckable(true);
    actMetricLoss->setCheckable(true);
    actMetricDelay->setChecked(true);
    metricGroup->addAction(actMetricDelay);
    metricGroup->addAction(actMetricBw);
    metricGroup->addAction(actMetricLoss);

    actShowPorts = new QAction(tr("Показывать номера портов"), this);
    actShowPorts->setCheckable(true);
    actShowPorts->setChecked(false);
    actShowPorts->setShortcut(QKeySequence("Ctrl+P"));
    connect(actShowPorts, &QAction::toggled, this, [this](bool on) {
        if (topoScene_) topoScene_->setShowPorts(on);
        emit signalShowPorts(on);    // совместимость со старым drawer'ом
    });

    connect(actToggleLog, &QAction::toggled, this, &MainWindow::onActionToggleLog);
    connect(actTogglePalette, &QAction::toggled, this, &MainWindow::onActionTogglePalette);
    connect(actZoomIn, &QAction::triggered, this, &MainWindow::onActionZoomIn);
    connect(actZoomOut, &QAction::triggered, this, &MainWindow::onActionZoomOut);
    connect(actZoomReset, &QAction::triggered, this, &MainWindow::onActionZoomReset);
    connect(actZoomFit, &QAction::triggered, this, &MainWindow::onActionZoomFit);
    connect(actToggleTheme, &QAction::triggered, this, &MainWindow::onActionToggleTheme);
    connect(actMetricDelay, &QAction::triggered, this, &MainWindow::onActionMetricDelay);
    connect(actMetricBw, &QAction::triggered, this, &MainWindow::onActionMetricBandwidth);
    connect(actMetricLoss, &QAction::triggered, this, &MainWindow::onActionMetricLoss);

    /* ===== Undo / Redo (#6) ===== */
    actUndo = new QAction(tm->makeIcon(":/modern/modern/edit.svg"),
                          tr("Отменить"), this);
    actUndo->setShortcut(QKeySequence::Undo);
    actUndo->setEnabled(false);
    actRedo = new QAction(tm->makeIcon(":/modern/modern/edit.svg"),
                          tr("Повторить"), this);
    actRedo->setShortcut(QKeySequence::Redo);
    actRedo->setEnabled(false);
    connect(actUndo, &QAction::triggered, this, [this]() {
        if (core_) core_->undo();
    });
    connect(actRedo, &QAction::triggered, this, [this]() {
        if (core_) core_->redo();
    });

    /* ===== Edit / layout ===== */
    actAlignH = new QAction(tm->makeIcon(":/modern/modern/align_h.svg"),
                            tr("Выровнять горизонтально"), this);
    actAlignH->setShortcut(QKeySequence("Shift+H"));
    actAlignV = new QAction(tm->makeIcon(":/modern/modern/align_v.svg"),
                            tr("Выровнять вертикально"), this);
    actAlignV->setShortcut(QKeySequence("Shift+V"));
    actClearMarks = new QAction(tm->makeIcon(":/modern/modern/clear_marks.svg"),
                                tr("Снять подсветку"), this);
    actDeleteSelected = new QAction(tr("Удалить выделенное"), this);
    actDeleteSelected->setShortcut(QKeySequence::Delete);

    connect(actAlignH, &QAction::triggered, this, &MainWindow::onActionAlignH);
    connect(actAlignV, &QAction::triggered, this, &MainWindow::onActionAlignV);
    connect(actClearMarks, &QAction::triggered, this, &MainWindow::onActionClearMarks);
    connect(actDeleteSelected, &QAction::triggered, this, &MainWindow::onActionDeleteSelected);

    /* ===== Algorithms ===== */
    actAlgorithms = new QAction(tm->makeIcon(":/modern/modern/algorithm.svg"),
                                tr("Запустить алгоритм…"), this);
    actAlgorithms->setShortcut(QKeySequence("Ctrl+R"));
    connect(actAlgorithms, &QAction::triggered, this, &MainWindow::onActionAlgorithms);

    actCreateUserAlgo = new QAction(tm->makeIcon(":/modern/modern/new.svg"),
                                    tr("Создать пользовательский алгоритм…"), this);
    actCreateUserAlgo->setShortcut(QKeySequence("Ctrl+Shift+N"));
    connect(actCreateUserAlgo, &QAction::triggered,
            this, &MainWindow::onActionCreateUserAlgorithm);

    actOpenAlgoFolder = new QAction(tm->makeIcon(":/modern/modern/open.svg"),
                                    tr("Открыть папку алгоритмов"), this);
    connect(actOpenAlgoFolder, &QAction::triggered,
            this, &MainWindow::onActionOpenUserAlgorithmsFolder);

    actOpenWebUI = new QAction(tm->makeIcon(":/modern/modern/run_ryu.svg"),
                               tr("Открыть веб-интерфейс контроллера"), this);
    actOpenWebUI->setShortcut(QKeySequence("Ctrl+W"));
    actOpenWebUI->setStatusTip(tr("Открыть http://localhost:8080 в браузере"));
    connect(actOpenWebUI, &QAction::triggered,
            this, &MainWindow::onActionOpenControllerWebUI);

    /* ===== Help ===== */
    actAbout = new QAction(tr("О программе"), this);
    actQuickStart = new QAction(tr("Быстрый старт"), this);
    actQuickStart->setShortcut(QKeySequence::HelpContents);
    connect(actAbout, &QAction::triggered, this, &MainWindow::onActionAbout);
    connect(actQuickStart, &QAction::triggered, this, &MainWindow::onActionQuickStart);
}

void MainWindow::createMenus()
{
    QMenuBar *bar = menuBar();

    QMenu *mFile = bar->addMenu(tr("Файл"));
    mFile->addAction(actNewProject);
    mFile->addAction(actOpenProject);
    mFile->addSeparator();
    mFile->addAction(actSave);
    mFile->addAction(actSaveAs);
    mFile->addSeparator();
    mFile->addAction(actExportMn);
    mFile->addAction(actExportMetrics);
    mFile->addSeparator();
    mFile->addAction(actExit);

    QMenu *mEdit = bar->addMenu(tr("Правка"));
    mEdit->addAction(actUndo);
    mEdit->addAction(actRedo);
    mEdit->addSeparator();
    mEdit->addAction(actDeleteSelected);
    mEdit->addAction(actClearMarks);
    mEdit->addSeparator();
    mEdit->addAction(actAlignH);
    mEdit->addAction(actAlignV);

    QMenu *mView = bar->addMenu(tr("Вид"));
    mView->addAction(actTogglePalette);
    mView->addAction(actToggleLog);
    mView->addSeparator();
    mView->addAction(actZoomIn);
    mView->addAction(actZoomOut);
    mView->addAction(actZoomReset);
    mView->addAction(actZoomFit);
    mView->addSeparator();
    QMenu *mMetric = mView->addMenu(tr("Метрика канала"));
    mMetric->addAction(actMetricDelay);
    mMetric->addAction(actMetricBw);
    mMetric->addAction(actMetricLoss);
    mView->addAction(actShowPorts);
    mView->addSeparator();
    mView->addAction(actToggleTheme);

    QMenu *mTopo = bar->addMenu(tr("Топология"));
    mTopo->addAction(actStart);
    mTopo->addAction(actStop);
    mTopo->addAction(actRestart);
    mTopo->addSeparator();
    mTopo->addAction(actPingAll);
    mTopo->addAction(actOpenWebUI);
    mTopo->addSeparator();
    mTopo->addAction(actRunRyu);
    mTopo->addAction(actRunMininet);
    mTopo->addAction(actOpenControllerCode);
    mTopo->addSeparator();
    mTopo->addAction(actMnCleanup);

    QMenu *mAlgo = bar->addMenu(tr("Алгоритмы"));
    mAlgo->addAction(actAlgorithms);
    mAlgo->addSeparator();
    mAlgo->addAction(actCreateUserAlgo);
    mAlgo->addAction(actOpenAlgoFolder);

    QMenu *mHelp = bar->addMenu(tr("Справка"));
    mHelp->addAction(actQuickStart);
    mHelp->addAction(actAbout);
}

void MainWindow::createMainToolBar()
{
    mainToolBar = addToolBar(tr("Основная панель"));
    mainToolBar->setObjectName("mainToolBar");
    mainToolBar->setMovable(false);
    mainToolBar->setIconSize(QSize(24, 24));
    mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    /* Project */
    mainToolBar->addAction(actNewProject);
    mainToolBar->addAction(actOpenProject);
    mainToolBar->addAction(actSave);
    mainToolBar->addSeparator();

    /* Topology lifecycle */
    mainToolBar->addAction(actStart);
    mainToolBar->addAction(actStop);
    mainToolBar->addAction(actPingAll);
    mainToolBar->addAction(actOpenWebUI);
    mainToolBar->addSeparator();

    /* Algorithms — точка входа в диалог. Старые комбо-боксы выбора
     * алгоритма и метрики на тулбаре убраны: всё это есть в самом
     * AlgorithmDialog и в меню «Вид → Метрика канала». */
    mainToolBar->addAction(actAlgorithms);
    mainToolBar->addAction(actClearMarks);
    mainToolBar->addSeparator();

    /* View */
    mainToolBar->addAction(actZoomOut);
    mainToolBar->addAction(actZoomReset);
    mainToolBar->addAction(actZoomIn);
    mainToolBar->addSeparator();

    /* Spacer */
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mainToolBar->addWidget(spacer);

    mainToolBar->addAction(actToggleTheme);

    addToolBar(Qt::TopToolBarArea, mainToolBar);
}

void MainWindow::createTerminalDock()
{
    terminal = new QTextBrowser(this);
    terminal->setObjectName("terminal");
    terminal->setReadOnly(true);

    terminalDock = new QDockWidget(tr("Журнал"), this);
    terminalDock->setObjectName("terminalDock");
    terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    terminalDock->setFeatures(QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetFloatable |
                              QDockWidget::DockWidgetClosable);
    terminalDock->setWidget(terminal);
    addDockWidget(Qt::BottomDockWidgetArea, terminalDock);
    /* Журнал ужимается до 40 px (одна строка) — удобно когда нужно
     * освободить место под холст. Стартовая высота 200 — достаточно,
     * чтобы было видно ~10 строк лога без скролла. Растягивать выше
     * можно за серый разделитель между журналом и холстом (стиль
     * QMainWindow::separator в обеих темах). */
    terminalDock->setMinimumHeight(40);
    resizeDocks({terminalDock}, {200}, Qt::Vertical);

    connect(terminalDock, &QDockWidget::visibilityChanged,
            actToggleLog, &QAction::setChecked);
}

void MainWindow::createStatusBar()
{
    QStatusBar *bar = statusBar();

    statsProjectLbl = new QLabel(tr("Проект: —"), bar);
    statsProjectLbl->setStyleSheet("font-weight:600;");
    statsHostsLbl = new QLabel(tr("Хосты: 0"), bar);
    statsSwitchesLbl = new QLabel(tr("Коммутаторы: 0"), bar);
    statsLinksLbl = new QLabel(tr("Каналы: 0"), bar);
    statsControllerLbl = new QLabel(tr("Контроллер: —"), bar);
    statsMininetLbl = new QLabel(tr("Mininet: не запущен"), bar);
    statsPingLbl = new QLabel(tr("Ping: —"), bar);
    statsZoomLbl = new QLabel(tr("100%"), bar);

    bar->addWidget(statsProjectLbl);
    bar->addWidget(statsHostsLbl);
    bar->addWidget(statsSwitchesLbl);
    bar->addWidget(statsLinksLbl);
    bar->addWidget(statsControllerLbl);
    bar->addPermanentWidget(statsMininetLbl);
    bar->addPermanentWidget(statsPingLbl);
    bar->addPermanentWidget(statsZoomLbl);
}

void MainWindow::refreshIcons()
{
    ThemeManager *tm = ThemeManager::instance();
    actNewProject->setIcon(tm->makeIcon(":/modern/modern/new.svg"));
    actOpenProject->setIcon(tm->makeIcon(":/modern/modern/open.svg"));
    actSave->setIcon(tm->makeIcon(":/modern/modern/save.svg"));
    actSaveAs->setIcon(tm->makeIcon(":/modern/modern/save_as.svg"));
    actExportMn->setIcon(tm->makeIcon(":/modern/modern/python.svg"));

    actStart->setIcon(tm->makeIcon(":/modern/modern/run_mininet.svg", tm->successColor()));
    actStop->setIcon(tm->makeIcon(":/modern/modern/clear_marks.svg", tm->dangerColor()));
    actPingAll->setIcon(tm->makeIcon(":/modern/modern/ping.svg", tm->successColor()));
    actRunRyu->setIcon(tm->makeIcon(":/modern/modern/run_ryu.svg"));
    actRunMininet->setIcon(tm->makeIcon(":/modern/modern/run_mininet.svg"));
    actOpenControllerCode->setIcon(tm->makeIcon(":/modern/modern/edit.svg"));

    actAlignH->setIcon(tm->makeIcon(":/modern/modern/align_h.svg"));
    actAlignV->setIcon(tm->makeIcon(":/modern/modern/align_v.svg"));
    actClearMarks->setIcon(tm->makeIcon(":/modern/modern/clear_marks.svg"));
    actAlgorithms->setIcon(tm->makeIcon(":/modern/modern/algorithm.svg"));
    actToggleTheme->setIcon(tm->makeIcon(":/modern/modern/theme.svg"));

    setWindowIcon(tm->makeIcon(":/modern/modern/app.svg", QSize(48, 48)));
}

void MainWindow::onThemeChanged()
{
    refreshIcons();
}

void MainWindow::setProjectTitle(const QString &name, const QString &dir)
{
    if (name.isEmpty() || dir.isEmpty())
    {
        setWindowTitle(tr("SDN Topology — Редактор ПКС"));
        statsProjectLbl->setText(tr("Проект: —"));
    }
    else
    {
        setWindowTitle(QString("SDN Topology — %0").arg(name));
        statsProjectLbl->setText(tr("Проект: %1").arg(name));
        statsProjectLbl->setToolTip(dir);
    }
}

/* ===== Palette → Scene tool sync ===== */
void MainWindow::onPaletteToolSelected(QString id)
{
    if (topoScene_) topoScene_->setActiveTool(id);
}


/* ===== Project / file ===== */
void MainWindow::onActionNewProject()
{
    NewProjectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    emit signalNewProject(dlg.fullProjectDir(),
                          dlg.projectName(),
                          int(dlg.chosenTemplate()));
}

void MainWindow::onActionOpenProject()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Откройте папку проекта"),
                                                    QDir::homePath());
    if (dir.isEmpty()) return;
    emit signalOpenProject(dir);
}

void MainWindow::onActionSave()
{
    if (openedProjectDir.isEmpty())
    {
        onActionNewProject();
        return;
    }
    emit signalSaveProject();
}

void MainWindow::onActionSaveAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Экспорт топологии (.xml)"),
                                                "topology.sdn.xml",
                                                tr("XML files (*.xml)"));
    if (path.isEmpty()) return;
    emit signalSaveNetworkMap(path);
}

void MainWindow::onActionExportMininet()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Экспорт Mininet-скрипта"),
                                                "topology.sdn.py",
                                                tr("Python files (*.py)"));
    if (path.isEmpty()) return;
    emit signalCreateMininetScript(path);
}

void MainWindow::onActionExportMetrics()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Экспорт матрицы метрик"),
                                                "metrics.txt",
                                                tr("Text files (*.txt)"));
    if (path.isEmpty()) return;
    emit signalCreateWeightMatrix(path);
}

void MainWindow::onActionExit()
{
    close();
}

/* ===== View ===== */
void MainWindow::onActionToggleLog(bool checked)
{
    if (terminalDock) terminalDock->setVisible(checked);
}
void MainWindow::onActionTogglePalette(bool checked)
{
    if (paletteDock_) paletteDock_->setVisible(checked);
}
void MainWindow::onActionZoomIn() { if (topoView_) topoView_->zoomIn(); }
void MainWindow::onActionZoomOut() { if (topoView_) topoView_->zoomOut(); }
void MainWindow::onActionZoomReset() { if (topoView_) topoView_->resetZoom(); }
void MainWindow::onActionZoomFit() { if (topoView_) topoView_->fitToContent(); }

void MainWindow::onActionToggleTheme()
{
    ThemeManager::instance()->toggleTheme();
}

void MainWindow::onActionMetricDelay()
{
    emit signalShowDelay();
    if (topoScene_) topoScene_->setMetric("delay");
}
void MainWindow::onActionMetricBandwidth()
{
    emit signalShowBandwidth();
    if (topoScene_) topoScene_->setMetric("bandwidth");
}
void MainWindow::onActionMetricLoss()
{
    emit signalShowPacketLossRate();
    if (topoScene_) topoScene_->setMetric("loss");
}

/* ===== Topology ===== */
void MainWindow::onActionStartTopology() { emit signalStartTopology(); }
void MainWindow::onActionStopTopology() { emit signalStopTopology(); }
void MainWindow::onActionRestartTopology()
{
    emit signalStopTopology();
    QTimer::singleShot(800, this, [this]() { emit signalStartTopology(); });
}
void MainWindow::onActionPingAll() { emit signalPingAll(); }
void MainWindow::onActionMnCleanup() { emit signalMnCleanup(); }
void MainWindow::onActionRunRyu() { emit signalRunRyu(); }
void MainWindow::onActionRunMininetOnly() { emit signalRunMininet(); }
void MainWindow::onActionOpenControllerCode() { emit signalOpenControllerCode(); }

/* ===== Edit / layout ===== */
void MainWindow::onActionAlignH() { emit signalAlignHorizontally(); }
void MainWindow::onActionAlignV() { emit signalAlignVertically(); }
void MainWindow::onActionClearMarks() { emit signalRemoveMarks(); }
void MainWindow::onActionDeleteSelected()
{
    /* Сначала пробуем удалить выделенное в новой сцене. */
    if (topoScene_)
    {
        topoScene_->deleteSelected();
        return;
    }
    /* Старый путь — на всякий случай. */
    emit signalHandleKeyDeletePressEvent();
}

void MainWindow::onActionAlgorithms() { emit signalOpenAlgorithmsDialog(); }

void MainWindow::onActionCreateUserAlgorithm()
{
    CreateAlgorithmDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    emit signalCreateUserAlgorithm(dlg.fileName(), dlg.className(), dlg.algorithmName());
}

void MainWindow::onActionOpenUserAlgorithmsFolder()
{
    emit signalOpenUserAlgorithmsFolder();
}

void MainWindow::onActionOpenControllerWebUI()
{
    emit signalOpenControllerWebUI();
}

/* ===== Help ===== */
void MainWindow::onActionAbout()
{
    QMessageBox::about(this, tr("О программе"),
        tr("<h3>SDN Topology</h3>"
           "<p>Визуальный редактор и среда исследования программно-конфигурируемых "
           "сетей (ПКС).</p>"
           "<p>Стек: Qt 5 / C++11, Mininet, Ryu, OpenFlow 1.3.</p>"
           "<p>Тема: Indigo Blue.</p>"));
}

void MainWindow::onActionQuickStart()
{
    QMessageBox::information(this, tr("Быстрый старт"),
        tr("<h3>Палитра инструментов</h3>"
           "Слева — палитра. Выберите инструмент:<br>"
           "• <b>Выбор (E)</b> — выделение/перетаскивание/рамка<br>"
           "• <b>Канал (L)</b> — соедините два узла кликами<br>"
           "• <b>Ластик (D)</b> — клик по элементу удаляет<br>"
           "• <b>Хост/Свитч/Контроллер</b> — клик на холсте создаёт. "
           "Можно также <i>перетащить</i> кнопку прямо на нужное место.<br><br>"
           "<h3>Холст</h3>"
           "• ЛКМ по пустому = рамка выделения<br>"
           "• Space+ЛКМ или средняя кнопка = панорамирование<br>"
           "• Колесо = zoom in/out<br>"
           "• Delete / Backspace = удалить выделенное<br>"
           "• Двойной клик по каналу = свойства (delay/bandwidth/loss)<br>"
           "• Двойной клик по узлу = свойства (IP, имя)<br>"
           "• Правый клик = контекстное меню (с удалением)<br><br>"
           "<h3>Запуск</h3>"
           "<b>F5</b> — Запустить Mininet + Ryu в терминалах.<br>"
           "<b>Ctrl+P</b> — Ping All (массовый ping всех хостов).<br>"
           "<b>Ctrl+W</b> — Открыть веб-интерфейс контроллера.<br>"
           "<b>Ctrl+R</b> — Запустить встроенный алгоритм маршрутизации.<br><br>"
           "<h3>Требования (Ubuntu)</h3>"
           "<code>sudo apt install mininet xterm gnome-terminal</code><br>"
           "<code>pip3 install ryu</code>"));
}

/* ===== Слоты от Core ===== */
void MainWindow::refreshNetworkMapView(QPixmap /*image*/)
{
    if (topoScene_) topoScene_->rebuildFromModel();
}

void MainWindow::showMessage(QString message)
{
    if (terminal)
    {
        terminal->append(QTime::currentTime().toString() + QString(" │ ") + message);
    }
}

void MainWindow::updateStats(int hosts, int switches, int links, int controllers)
{
    statsHostsLbl->setText(tr("Хосты: %1").arg(hosts));
    statsSwitchesLbl->setText(tr("Коммутаторы: %1").arg(switches));
    statsLinksLbl->setText(tr("Каналы: %1").arg(links));
    if (controllers <= 0)
        statsControllerLbl->setText(tr("Контроллер: —"));
    else if (controllers == 1)
        statsControllerLbl->setText(tr("Контроллер: c0"));
    else
        statsControllerLbl->setText(tr("Контроллеров: %1").arg(controllers));
}

void MainWindow::updatePingStats(int success, int total)
{
    if (total == 0)
    {
        statsPingLbl->setText(tr("Ping: —"));
        return;
    }
    int pct = (success * 100) / total;
    statsPingLbl->setText(tr("Ping: %1% (%2/%3)").arg(pct).arg(success).arg(total));
}

void MainWindow::setMininetStatus(const QString &status)
{
    statsMininetLbl->setText(tr("Mininet: %1").arg(status));
}

void MainWindow::onProjectChanged(QString dir, QString name)
{
    openedProjectDir = dir;
    openedProjectName = name;
    setProjectTitle(name, dir);
}

void MainWindow::onHostContextRequested(QString hostName, QStringList others, QPoint globalPos)
{
    Q_UNUSED(others);
    ThemeManager *tm = ThemeManager::instance();
    QMenu menu(this);
    QAction *titleAct = menu.addAction(tr("Хост: %1").arg(hostName));
    titleAct->setEnabled(false);
    menu.addSeparator();

    QAction *xtermAct = menu.addAction(
        tm->makeIcon(":/modern/modern/run_mininet.svg", tm->accentColor(), QSize(20, 20)),
        tr("Открыть xterm"));
    menu.addSeparator();
    QAction *delAct = menu.addAction(
        tm->makeIcon(":/modern/modern/clear_marks.svg", tm->dangerColor(), QSize(20, 20)),
        tr("Удалить хост"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == xtermAct)
    {
        emit signalOpenHostXterm(hostName);
    }
    else if (chosen == delAct)
    {
        if (core_ && core_->getMap())
        {
            core_->pushUndoState();
            for (Host *h : core_->getMap()->getHosts())
            {
                if (h->getName() == hostName)
                {
                    core_->getMap()->removeElement(h);
                    break;
                }
            }
            if (topoScene_) topoScene_->rebuildFromModel();
            onSceneModelChanged();
        }
    }
}

void MainWindow::onControllerContextRequested(QPoint globalPos)
{
    ThemeManager *tm = ThemeManager::instance();
    QMenu menu(this);
    QAction *titleAct = menu.addAction(tr("Контроллер"));
    titleAct->setEnabled(false);
    menu.addSeparator();
    QAction *runAct = menu.addAction(
        tm->makeIcon(":/modern/modern/run_ryu.svg", tm->accentColor(), QSize(20, 20)),
        tr("Запустить Ryu"));
    QAction *codeAct = menu.addAction(
        tm->makeIcon(":/modern/modern/edit.svg", QSize(20, 20)),
        tr("Открыть controller.py"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == runAct) emit signalRunRyu();
    else if (chosen == codeAct) emit signalOpenControllerCode();
}

void MainWindow::applyZoomLabel()
{
    if (statsZoomLbl && topoView_)
        statsZoomLbl->setText(QString("%1%").arg(qRound(topoView_->zoom() * 100)));
}

void MainWindow::onSceneNodeContextMenu(NodeItem *n, QPoint globalPos)
{
    if (!n || !n->domainNode()) return;
    Node *dn = n->domainNode();
    DeviceType dt = dn->getDeviceType();
    ThemeManager *tm = ThemeManager::instance();

    if (dt == HOST)
    {
        Host *clicked = static_cast<Host *>(dn);
        QStringList others;
        if (core_ && core_->getMap())
        {
            for (Host *h : core_->getMap()->getHosts())
            {
                if (h == clicked) continue;
                others << h->getName();
            }
        }
        onHostContextRequested(clicked->getName(), others, globalPos);
        return;
    }
    if (dt == SDNCONTROLLER)
    {
        QMenu menu(this);
        QAction *titleAct = menu.addAction(tr("Контроллер: %1").arg(dn->getName()));
        titleAct->setEnabled(false);
        menu.addSeparator();
        QAction *propsAct = menu.addAction(tr("Свойства…"));
        QAction *runAct = menu.addAction(
            tm->makeIcon(":/modern/modern/run_ryu.svg", tm->accentColor(), QSize(20, 20)),
            tr("Запустить Ryu"));
        QAction *codeAct = menu.addAction(
            tm->makeIcon(":/modern/modern/edit.svg", QSize(20, 20)),
            tr("Открыть controller.py"));
        menu.addSeparator();
        QAction *delAct = menu.addAction(
            tm->makeIcon(":/modern/modern/clear_marks.svg", tm->dangerColor(), QSize(20, 20)),
            tr("Удалить контроллер"));
        QAction *chosen = menu.exec(globalPos);
        if (chosen == propsAct) { dn->configure(); if (topoScene_) topoScene_->rebuildFromModel(); }
        else if (chosen == runAct) emit signalRunRyu();
        else if (chosen == codeAct) emit signalOpenControllerCode();
        else if (chosen == delAct)
        {
            if (core_ && core_->getMap())
            {
                core_->getMap()->removeElement(dn);
                if (topoScene_) topoScene_->rebuildFromModel();
                onSceneModelChanged();
            }
        }
        return;
    }
    if (dt == SWITCH)
    {
        QMenu menu(this);
        QAction *titleAct = menu.addAction(tr("Коммутатор: %1").arg(dn->getName()));
        titleAct->setEnabled(false);
        menu.addSeparator();
        bool on = core_ && core_->isSwitchRouteHighlighted(dn->getName());
        QAction *routesAct = menu.addAction(
            tm->makeIcon(":/modern/modern/algorithm.svg", tm->accentColor(), QSize(20, 20)),
            on ? tr("Убрать маршруты этого свича")
               : tr("Подсветить маршруты от этого свича"));
        menu.addSeparator();
        QAction *delAct = menu.addAction(
            tm->makeIcon(":/modern/modern/clear_marks.svg", tm->dangerColor(), QSize(20, 20)),
            tr("Удалить коммутатор"));
        QAction *chosen = menu.exec(globalPos);
        if (chosen == routesAct) { if (core_) core_->highlightRoutesFromSwitch(dn->getName()); }
        else if (chosen == delAct)
        {
            if (core_ && core_->getMap())
            {
                core_->getMap()->removeElement(dn);
                if (topoScene_) topoScene_->rebuildFromModel();
                onSceneModelChanged();
            }
        }
        return;
    }
    if (dt == DOCKERNODE)
    {
        QMenu menu(this);
        QAction *titleAct = menu.addAction(tr("Docker: %1").arg(dn->getName()));
        titleAct->setEnabled(false);
        menu.addSeparator();
        QAction *propsAct = menu.addAction(tr("Свойства…"));
        QAction *shellAct = menu.addAction(
            tm->makeIcon(":/modern/modern/terminal.svg", tm->accentColor(), QSize(20, 20)),
            tr("Войти в контейнер (docker exec)"));
        QAction *logsAct = menu.addAction(
            tm->makeIcon(":/modern/modern/log.svg", tm->accentColor(), QSize(20, 20)),
            tr("Просмотр логов (docker logs)"));
        menu.addSeparator();
        QAction *delAct = menu.addAction(
            tm->makeIcon(":/modern/modern/clear_marks.svg", tm->dangerColor(), QSize(20, 20)),
            tr("Удалить контейнер"));
        QAction *chosen = menu.exec(globalPos);
        if (chosen == propsAct)
        {
            if (core_) core_->pushUndoState();
            dn->configure();
            if (topoScene_) topoScene_->rebuildFromModel();
        }
        else if (chosen == shellAct)
        {
            /* Containernet даёт контейнерам имена с префиксом "mn." (mn.d1).
             * docker.sock доступен только root → используем sudo с askpass
             * (тем же что для Mininet). */
            if (!core_ || !core_->getLauncher() || !core_->getLauncher()->hasSudoPassword())
            {
                QMessageBox::warning(this, tr("Войти в контейнер"),
                    tr("Сначала запустите топологию — нужен пароль sudo "
                       "для доступа к docker.sock."));
                return;
            }
            QString askpass = core_->getLauncher()->createAskpassScript();
            QString cmd = QString(
                "export SUDO_ASKPASS='%2'; "
                "trap \"rm -f '%2'\" EXIT; "
                "sudo -A docker exec -it mn.%1 bash 2>/dev/null || "
                "sudo -A docker exec -it mn.%1 sh; "
                "echo; echo '--- завершено, Enter для закрытия ---'; read"
            ).arg(dn->getName(), askpass);
            QProcess::startDetached("xterm", {"-T", QString("docker: %1").arg(dn->getName()),
                                              "-e", "bash", "-c", cmd});
        }
        else if (chosen == logsAct)
        {
            if (!core_ || !core_->getLauncher() || !core_->getLauncher()->hasSudoPassword())
            {
                QMessageBox::warning(this, tr("Логи контейнера"),
                    tr("Сначала запустите топологию — нужен пароль sudo."));
                return;
            }
            QString askpass = core_->getLauncher()->createAskpassScript();
            QString cmd = QString(
                "export SUDO_ASKPASS='%2'; "
                "trap \"rm -f '%2'\" EXIT; "
                "sudo -A docker logs -f mn.%1 2>&1; "
                "echo; echo '--- завершено, Enter для закрытия ---'; read"
            ).arg(dn->getName(), askpass);
            QProcess::startDetached("xterm", {"-T", QString("logs: %1").arg(dn->getName()),
                                              "-e", "bash", "-c", cmd});
        }
        else if (chosen == delAct)
        {
            if (core_ && core_->getMap())
            {
                core_->pushUndoState();
                core_->getMap()->removeElement(dn);
                if (topoScene_) topoScene_->rebuildFromModel();
                onSceneModelChanged();
            }
        }
        return;
    }
}

void MainWindow::onSceneLinkContextMenu(LinkItem *li, QPoint globalPos)
{
    if (!li || !li->domainLink()) return;
    Link *l = li->domainLink();
    ThemeManager *tm = ThemeManager::instance();
    QMenu menu(this);

    QString title;
    SSLink *sl = dynamic_cast<SSLink *>(l);
    CSLink *cl = dynamic_cast<CSLink *>(l);
    if (sl) title = tr("Канал: %1 — %2")
                    .arg(l->getNode1() ? l->getNode1()->getName() : "?")
                    .arg(l->getNode2() ? l->getNode2()->getName() : "?");
    else if (cl) title = tr("Связь контроллера: %1 — %2")
                          .arg(l->getNode1() ? l->getNode1()->getName() : "?")
                          .arg(l->getNode2() ? l->getNode2()->getName() : "?");
    else title = tr("Канал");

    QAction *titleAct = menu.addAction(title);
    titleAct->setEnabled(false);
    menu.addSeparator();
    QAction *propsAct = nullptr;
    QAction *toggleAct = nullptr;
    if (sl)
    {
        propsAct = menu.addAction(tr("Свойства (delay/bw/loss)…"));
        toggleAct = menu.addAction(sl->isDisabled()
                                   ? tr("Включить канал (link up)")
                                   : tr("Отключить канал (link down)"));
    }
    menu.addSeparator();
    QAction *delAct = menu.addAction(
        tm->makeIcon(":/modern/modern/clear_marks.svg", tm->dangerColor(), QSize(20, 20)),
        tr("Удалить канал"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == propsAct && sl)
    {
        if (core_) core_->pushUndoState();
        sl->configure();
        if (topoScene_)
        {
            li->update();
            /* Метрика отображения линка управляется чекаемыми пунктами
             * View → Метрика канала; берём активный. */
            QString m = "delay";
            if (actMetricBw && actMetricBw->isChecked()) m = "bandwidth";
            else if (actMetricLoss && actMetricLoss->isChecked()) m = "loss";
            topoScene_->setMetric(m);
        }
        /* Авто-пересчёт активного алгоритма. */
        if (core_) core_->recomputeActiveRouting();
    }
    else if (chosen == toggleAct && sl)
    {
        if (core_) core_->pushUndoState();
        bool nowDisabled = !sl->isDisabled();
        sl->setDisabled(nowDisabled);
        li->update();
        QString a = l->getNode1() ? l->getNode1()->getName() : QString();
        QString b = l->getNode2() ? l->getNode2()->getName() : QString();
        if (core_ && core_->getHttpClient() && !a.isEmpty() && !b.isEmpty())
        {
            /* up == !disabled. Mininet net.configLinkStatus + Ryu сам
             * пересчитает flow по EventLinkDelete/Add. */
            core_->getHttpClient()->setLinkStatus(a, b, !nowDisabled);
        }
        statusBar()->showMessage(nowDisabled
                                 ? tr("Канал %1—%2 отключён").arg(a, b)
                                 : tr("Канал %1—%2 включён").arg(a, b), 4000);
        if (core_) core_->recomputeActiveRouting();
    }
    else if (chosen == delAct)
    {
        if (core_ && core_->getMap())
        {
            core_->pushUndoState();
            core_->getMap()->removeElement(l);
            if (topoScene_) topoScene_->rebuildFromModel();
            onSceneModelChanged();
        }
    }
}

void MainWindow::onSceneEmptyContextMenu(QPoint globalPos)
{
    QMenu menu(this);
    QAction *aHost = menu.addAction(tr("Добавить хост сюда"));
    QAction *aSw   = menu.addAction(tr("Добавить коммутатор сюда"));
    QAction *aC0   = menu.addAction(tr("Добавить контроллер сюда"));
    QAction *aDk   = menu.addAction(tr("Добавить Docker-контейнер сюда"));
    menu.addSeparator();
    QAction *aFit  = menu.addAction(tr("Подогнать по окну"));
    QAction *aRst  = menu.addAction(tr("Сбросить масштаб (100%)"));
    QAction *chosen = menu.exec(globalPos);
    if (!chosen) return;
    QPointF scenePos = topoView_
                       ? topoView_->mapToScene(topoView_->mapFromGlobal(globalPos))
                       : QPointF(0, 0);
    if (chosen == aHost && topoScene_) topoScene_->createNodeAt("host", scenePos);
    else if (chosen == aSw && topoScene_) topoScene_->createNodeAt("switch", scenePos);
    else if (chosen == aC0 && topoScene_) topoScene_->createNodeAt("controller", scenePos);
    else if (chosen == aDk && topoScene_) topoScene_->createNodeAt("docker", scenePos);
    else if (chosen == aFit && topoView_) topoView_->fitToContent();
    else if (chosen == aRst && topoView_) topoView_->resetZoom();
}

void MainWindow::onSceneMessage(QString msg)
{
    showMessage(msg);
}

void MainWindow::onSceneModelChanged()
{
    if (core_ && core_->getMap())
    {
        updateStats(core_->getMap()->getHostCount(),
                    core_->getMap()->getSwitchCount(),
                    core_->getMap()->getLinkCount(),
                    core_->getMap()->getSdnControllerCount());
    }
}
