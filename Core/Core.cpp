#include "Core.h"
#include "IO.h"
#include "XmlDeserializer.h"
#include "WeightsMatrix.h"
#include "DrawingMode.h"
#include "ProcessLauncher.h"
#include "MininetHttpClient.h"
#include "ProjectManager.h"
#include "SSLink.h"
#include "Switch.h"
#include "Host.h"
#include "SdnController.h"
#include "AlgorithmRegistry.h"
#include "IRoutingAlgorithm.h"
#include "LARACAlgorithm.h"
#include "Graph.h"
#include "CommutationMatrix.h"
#include "SearchEngine.h"
#include <QDesktopServices>
#include <QUrl>

#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
#include <QInputDialog>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

Core::Core(QObject *parent) :
    QObject(parent),
    nodeCreator(&map),
    linkCreator(&map),
    elementEditor(&map),
    launcher(new ProcessLauncher(this)),
    mnHttp(new MininetHttpClient(this)),
    project(new ProjectManager(this)),
    ryuNam_(new QNetworkAccessManager(this)),
    mininetHttpPort(5555),
    mininetRunning(false),
    currentAlgoId_("dijkstra"),
    currentMetric_("delay")
{
    changeStateToEdit();
    DrawingMode::showDelay();

    QSettings s("SNet", "Editor");
    mininetHttpPort = static_cast<quint16>(s.value("mininet/http_port", 5555).toUInt());
    mnHttp->setBaseUrl(QString("http://127.0.0.1:%1").arg(mininetHttpPort));

    connect(launcher, &ProcessLauncher::launched,
            this, &Core::onMininetLaunched);
    connect(launcher, &ProcessLauncher::failed,
            this, &Core::onMininetFailed);
    /* Когда `sudo mn -c` завершён (mininetCleaned) — стартуем Ryu, а через
     * 3 секунды — Mininet. Подключение однократное (Qt::UniqueConnection
     * нельзя у лямбды без явного receiver, поэтому disconnect-and-reconnect
     * не нужен — каждый startTopology переподключает). */
    connect(mnHttp, &MininetHttpClient::pingAllResult,
            this, &Core::onPingAllReceived);
    connect(mnHttp, &MininetHttpClient::requestFailed,
            this, [this](const QString &endpoint, const QString &reason) {
                /* Fallback к `sudo python3 --pingall` запускаем ТОЛЬКО для
                 * pingall. Иначе любая ошибка /traffic_events (500мс polling)
                 * каскадно запускает sudo-процессы в бесконечном цикле. */
                if (endpoint == "pingall")
                {
                    onPingHttpFailed(QString("%1: %2").arg(endpoint).arg(reason));
                }
                else
                {
                    /* Тихо логируем — пользователю не интересно, что
                     * Mininet ещё не стартовал. */
                    static int suppressCount = 0;
                    if (++suppressCount % 20 == 1)
                    {
                        emit signalMessage(tr("HTTP %1: %2 (подавлено %3 ошибок)")
                                           .arg(endpoint).arg(reason).arg(suppressCount));
                    }
                }
            });
}

Core::~Core()
{
}

NetworkMap *Core::getMap()
{
    return &map;
}

void Core::refreshNetworkMap()
{
    emit signalRefreshNetworkMapView(map.draw());
    emitStats();
}

void Core::emitStats()
{
    emit signalStatsChanged(map.getHostCount(),
                            map.getSwitchCount(),
                            map.getLinkCount(),
                            map.getSdnControllerCount());
}

void Core::handleMouseReleaseEvent(QPoint position)
{
    tool->handleMouseUp(position);
    refreshNetworkMap();
}

void Core::handleMousePressEvent(QPoint position)
{
    tool->handleMouseDown(position);
    refreshNetworkMap();
}

void Core::handleMouseMoveEvent(QPoint position)
{
    if (tool->handleMouseMove(position))
    {
        refreshNetworkMap();
    }
}

void Core::handleDoubleClickEvent(QPoint position)
{
    tool->handleMouseDoubleClicked(position);
    refreshNetworkMap();
}

void Core::handleKeyDeletePressEvent()
{
    tool->handleKeyDeletePressEvent();
    refreshNetworkMap();
}

void Core::changeStateToEdit()
{
    tool = &elementEditor;
}

void Core::prepareSdnController()
{
    tool = &nodeCreator;
    nodeCreator.setSdnControllerCreation();
}

void Core::prepareHost()
{
    tool = &nodeCreator;
    nodeCreator.setHostCreation();
}

void Core::prepareSwitch()
{
    tool = &nodeCreator;
    nodeCreator.setSwitchCreation();
}

void Core::prepareLink()
{
    tool = &linkCreator;
}

void Core::prepareTextLabel()
{
    tool = &nodeCreator;
    nodeCreator.setTextLabelCreation();
}

void Core::showPorts(bool status)
{
    DrawingMode::needShowPorts = status;
    refreshNetworkMap();
}

void Core::showBandwidth()
{
    DrawingMode::showBandwidth();
    refreshNetworkMap();
}

void Core::showDelay()
{
    DrawingMode::showDelay();
    refreshNetworkMap();
}

void Core::showPacketLossRate()
{
    DrawingMode::showPacketLossRate();
    refreshNetworkMap();
}

void Core::createMininetScript(QString filePath)
{
    IO io;
    io.writeFile(map.createMininetScript(), filePath);
}

void Core::saveNetworkMap(QString filePath)
{
    IO io;
    io.writeFile(map.createXmlDocument(), filePath);
}

void Core::loadNetworkMap(QString filePath)
{
    IO io;
    XmlDeserializer deserializer;
    QString xmlDocument = io.readFile(filePath);
    deserializer.deserialize(xmlDocument, &map);
    elementEditor.handleMouseDown(QPoint(-999, -999));
    refreshNetworkMap();
    resetUndoHistory();
}

void Core::visualizePath(QList<int> path)
{
    map.unselectLinks();
    map.visualizePath(path);
    refreshNetworkMap();
    emit signalHighlightPath(path, QColor("#DC2626"));
}

void Core::visualizePaths(QList<QList<int> > paths)
{
    map.unselectLinks();
    map.visualizePaths(paths);
    refreshNetworkMap();
    /* На canvas пока подсвечиваем все пути одним цветом. */
    QList<QList<int>> tree;
    for (const QList<int> &p : paths)
    {
        for (int i = 0; i + 1 < p.size(); ++i)
        {
            tree.append({p[i], p[i + 1]});
        }
    }
    emit signalHighlightTree(tree, QColor("#2563EB"));
}

void Core::visualizeIslands(QList<QList<int> > islands)
{
    map.unselectNodes();
    map.visualizeIslands(islands);
    refreshNetworkMap();
}

void Core::visualizeTree(QList<QList<int> > tree)
{
    map.unselectLinks();
    map.visualizeTree(tree);
    refreshNetworkMap();
    emit signalHighlightTree(tree, QColor("#DC2626"));
}

void Core::visualizeTrees(QList<QList<QList<int> > > trees)
{
    map.unselectLinks();
    map.visualizeTrees(trees);
    refreshNetworkMap();
    QList<QList<int>> flat;
    for (const auto &t : trees) flat.append(t);
    emit signalHighlightTree(flat, QColor("#7C3AED"));
}

void Core::eraseMarks()
{
    map.unselectLinks();
    map.unselectNodes();
    routeRoots_.clear();
    routeColorIdx_.clear();
    routeNextColor_ = 0;
    refreshNetworkMap();
    emit signalClearHighlights();
}

void Core::changeMetric(QVector<float> metricData)
{
    map.changeMetric(metricData);
    refreshNetworkMap();
}

void Core::AlignVertically()
{
    pushUndoState();
    elementEditor.AlignVertically();
    refreshNetworkMap();
    emit signalModelRestored();
}

void Core::AlignHorizontally()
{
    pushUndoState();
    elementEditor.AlignHorizontally();
    refreshNetworkMap();
    emit signalModelRestored();
}

void Core::createWeightsMatrix(QString path)
{
    WeightsMatrix matrix;
    IO io;
    QString content = matrix.build(map.getGraphMatrix(), map.getSwitches());
    io.writeFile(content, path);
}

void Core::connectSdnController()
{
    map.connectSdnController();
    refreshNetworkMap();
}

void Core::clearNetworkMap()
{
    elementEditor.handleMouseDown(QPoint(-999, -999));
    map.clear();
    refreshNetworkMap();
    resetUndoHistory();
}

QString Core::buildAndSaveTempMininetScript()
{
    /* Если открыт проект — кладём скрипт прямо в его папку (всегда свежий). */
    if (project && project->isOpen())
    {
        IO io;
        QString path = project->mininetScriptPath();
        io.writeFile(map.createMininetScript(), path);
        tempMininetScriptPath = path;
        return path;
    }
    /* Иначе — временный файл в TempLocation. */
    QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    QString path = QDir(dir).filePath("snet_topology.sdn.py");
    IO io;
    io.writeFile(map.createMininetScript(), path);
    tempMininetScriptPath = path;
    return path;
}

void Core::launchMininet()
{
    if (map.getSwitchCount() == 0 && map.getHostCount() == 0)
    {
        emit signalMessage(tr("Топология пуста — нечего запускать в Mininet"));
        return;
    }
    QString scriptPath = buildAndSaveTempMininetScript();
    QStringList args;
    args << "--http" << QString::number(mininetHttpPort);

    QString err;
    if (launcher->launchMininet(scriptPath, args, &err))
    {
        emit signalMessage(tr("Mininet запускается… (%1, HTTP-порт %2)")
                           .arg(QFileInfo(scriptPath).fileName())
                           .arg(mininetHttpPort));
        emit signalMininetStatusChanged(tr("запускается"));
        mininetRunning = true;
    }
    else
    {
        emit signalMessage(tr("Ошибка запуска Mininet: %1").arg(err));
        emit signalMininetStatusChanged(tr("ошибка"));
    }
}

void Core::launchRyu()
{
    QString ryuApp;
    /* Приоритет: контроллер из открытого проекта, иначе сохранённый путь из QSettings. */
    if (project && project->isOpen())
    {
        ryuApp = project->controllerPyPath();
    }
    if (ryuApp.isEmpty() || !QFileInfo::exists(ryuApp))
    {
        QSettings s("SNet", "Editor");
        ryuApp = s.value("ryu/last_app_path").toString();
    }
    if (ryuApp.isEmpty() || !QFileInfo::exists(ryuApp))
    {
        emit signalMessage(tr("Файл controller.py не найден. Откройте/создайте проект."));
        return;
    }
    QString err;
    if (launcher->launchRyu(ryuApp, &err))
    {
        emit signalMessage(tr("Ryu запускается… (%1)").arg(QFileInfo(ryuApp).fileName()));
    }
    else
    {
        emit signalMessage(tr("Ошибка запуска Ryu: %1").arg(err));
    }
}

bool Core::ensureSudoPassword()
{
    /* Запрашиваем пароль один раз за сессию. Хранится в RAM ProcessLauncher,
     * через stdin sudo -S — не виден в `ps`. */
    if (launcher->hasSudoPassword())
        return true;
    bool ok = false;
    QString pw = QInputDialog::getText(
        nullptr,
        tr("Пароль sudo"),
        tr("Для запуска Mininet нужен пароль sudo.\n"
           "Пароль хранится только в памяти процесса\nи не виден в `ps`."),
        QLineEdit::Password,
        QString(),
        &ok);
    if (!ok || pw.isEmpty())
    {
        emit signalMessage(tr("Пароль sudo не введён — старт отменён."));
        return false;
    }
    launcher->setSudoPassword(pw);
    /* Проверочный sudo -S true: если пароль неверный, поймём сразу. */
    QProcess test;
    test.start("sudo", QStringList() << "-S" << "-p" << "" << "-k" << "true");
    test.waitForStarted(2000);
    test.write((pw + "\n").toUtf8());
    test.closeWriteChannel();
    if (!test.waitForFinished(5000) || test.exitCode() != 0)
    {
        launcher->clearSudoPassword();
        emit signalMessage(tr("Пароль sudo не подошёл. Попробуйте ещё раз."));
        return false;
    }
    return true;
}

void Core::startTopology()
{
    emit signalMessage(tr("=== Старт топологии ==="));

    /* Автосохранение перед запуском: фиксируем текущую топологию в проект,
     * чтобы сгенерированный topology.sdn.py соответствовал тому, что на
     * холсте, и чтобы случайный сбой Mininet не потерял правки. */
    if (project && project->isOpen())
    {
        QString err;
        if (project->saveAll(&map, &err))
            emit signalMessage(tr("Автосохранение: проект сохранён перед стартом"));
        else
            emit signalMessage(tr("Автосохранение не удалось: %1").arg(err));
    }

    if (!ensureSudoPassword())
    {
        emit signalMininetStatusChanged(tr("остановлен"));
        return;
    }

    /* Гарантируем чистый старт. ВСЕГДА убиваем зомби-процессы из прошлых
     * сессий (могут оставаться от каскада fallback-pingall'ов из старых
     * багов, или при крэше предыдущего SNet). killMininetScript делает
     * глобальный pkill -9 -f 'topology.sdn.py', что убирает всех зомби. */
    emit signalMessage(tr("Очистка зомби-процессов прошлых сессий…"));
    launcher->killRyu();
    launcher->killMininetScript();

    /* Фазированный запуск:
     *   1. sudo -S mn -c (фоном) — чистит ОС от остатков Mininet.
     *      Одновременно делает killall ryu-manager, но Ryu ещё не стартовал.
     *   2. Когда mn -c завершился → запускаем Ryu.
     *   3. Через 3 секунды → запускаем Mininet.
     * Подключение mininetCleaned — UniqueConnection, чтобы повторные нажатия
     * Start не создавали каскада callback'ов. */
    disconnect(launcher, &ProcessLauncher::mininetCleaned, this, nullptr);
    connect(launcher, &ProcessLauncher::mininetCleaned, this,
            &Core::onMininetCleanupFinished, Qt::UniqueConnection);

    emit signalMessage(tr("[1/3] Очистка Mininet (sudo mn -c)…"));
    emit signalMininetStatusChanged(tr("очистка…"));
    launcher->runMininetCleanup(tr("перед стартом"));
}

void Core::onMininetCleanupFinished()
{
    /* Защита: если onMininetCleanupFinished вызвался дважды (stop за start —
     * случается при «починили stop»), не запускаем Ryu повторно. */
    disconnect(launcher, &ProcessLauncher::mininetCleaned, this,
               &Core::onMininetCleanupFinished);

    emit signalMessage(tr("[2/3] Запуск Ryu-контроллера…"));
    launchRyu();
    /* Ждём пока Ryu забиндит OpenFlow-порт 6653 и WSGI 8080. */
    QTimer::singleShot(3000, this, [this]() {
        emit signalMessage(tr("[3/3] Запуск Mininet-топологии…"));
        launchMininet();
        /* Активная маршрутизация по алгоритму временно отключена —
         * раньше она ломала table-miss flow. Контроллер сейчас работает
         * как простой L2 learning switch. Селектор алгоритма в тулбаре
         * сохраняет выбор для будущей реализации. */
    });
}

void Core::stopTopology()
{
    emit signalMessage(tr("=== Остановка топологии ==="));
    /* Stop делается полностью неблокирующе: HTTP shutdown → kill Ryu → kill Mininet
     * → sudo mn -c. Каждый шаг — async, без зависаний UI. */
    mnHttp->shutdown();
    launcher->killRyu();
    emit signalMessage(tr("ryu-manager убит (pkill)."));
    if (launcher->hasSudoPassword())
    {
        launcher->killMininetScript();
        emit signalMessage(tr("Mininet-процесс убит (sudo pkill -P pid)."));
        /* mn -c для чистки ovs-мостов и интерфейсов — фоном. */
        disconnect(launcher, &ProcessLauncher::mininetCleaned, this, nullptr);
        launcher->runMininetCleanup(tr("остановка"));
    }
    else
    {
        emit signalMessage(tr("Нет сохранённого пароля sudo — пропускаю mn -c."));
    }
    emit signalMininetStatusChanged(tr("остановлен"));
    mininetRunning = false;
}

void Core::openHostXterm(QString hostName)
{
    emit signalMessage(tr("Открываю xterm для %1…").arg(hostName));
    mnHttp->openXterm(hostName);
}

void Core::openControllerCode()
{
    QString path;
    if (project && project->isOpen())
    {
        path = project->controllerPyPath();
    }
    if (path.isEmpty() || !QFileInfo::exists(path))
    {
        QSettings s("SNet", "Editor");
        path = s.value("ryu/last_app_path").toString();
    }
    if (path.isEmpty() || !QFileInfo::exists(path))
    {
        emit signalMessage(tr("ryu_app_controller.py не найден. Создайте проект."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    emit signalMessage(tr("Открыт %1").arg(path));
}

void Core::openControllerWebUI()
{
    /* Ryu WSGI слушает 8080 по умолчанию. */
    QSettings s("SNet", "Editor");
    int port = s.value("ryu/wsgi_port", 8080).toInt();
    QString url = QString("http://localhost:%1").arg(port);
    QDesktopServices::openUrl(QUrl(url));
    emit signalMessage(tr("Открываю %1 в браузере").arg(url));
}

void Core::createUserAlgorithm(QString fileName, QString className, QString algoName)
{
    if (!project || !project->isOpen())
    {
        emit signalMessage(tr("Проект не открыт. Создайте новый или откройте существующий."));
        return;
    }
    QString err;
    QString path = project->createUserAlgorithm(fileName, className, algoName, &err);
    if (path.isEmpty())
    {
        emit signalMessage(tr("Не удалось создать алгоритм: %1").arg(err));
        return;
    }
    emit signalMessage(tr("Создан %1. Откройте файл в редакторе и реализуйте метод run().")
                       .arg(path));
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void Core::openUserAlgorithmsFolder()
{
    if (!project || !project->isOpen())
    {
        emit signalMessage(tr("Проект не открыт"));
        return;
    }
    QString dir = project->userAlgorithmsDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void Core::newProjectFromTemplate(QString projectDir, QString name, int tmplInt)
{
    elementEditor.handleMouseDown(QPoint(-999, -999));
    QString err;
    bool ok = project->createProject(projectDir, name,
                                     static_cast<ProjectManager::Template>(tmplInt),
                                     &map, &err);
    if (!ok)
    {
        emit signalMessage(tr("Не удалось создать проект: %1").arg(err));
        return;
    }
    emit signalProjectChanged(project->projectDir(), project->projectName());
    emit signalMessage(tr("Создан проект «%1» → %2").arg(name).arg(projectDir));
    refreshNetworkMap();
    resetUndoHistory();
}

void Core::openProjectFromDir(QString projectDir)
{
    elementEditor.handleMouseDown(QPoint(-999, -999));
    QString err;
    if (!project->openProject(projectDir, &map, &err))
    {
        emit signalMessage(tr("Не удалось открыть проект: %1").arg(err));
        return;
    }
    emit signalProjectChanged(project->projectDir(), project->projectName());
    emit signalMessage(tr("Открыт проект «%1» (%2)").arg(project->projectName()).arg(projectDir));
    refreshNetworkMap();
    resetUndoHistory();
}

void Core::saveProjectAll()
{
    if (!project->isOpen())
    {
        emit signalMessage(tr("Нет открытого проекта"));
        return;
    }
    QString err;
    if (project->saveAll(&map, &err))
    {
        emit signalMessage(tr("Проект сохранён: %1").arg(project->projectDir()));
    }
    else
    {
        emit signalMessage(tr("Ошибка сохранения: %1").arg(err));
    }
}

void Core::closeCurrentProject()
{
    project->closeProject();
    emit signalProjectChanged(QString(), QString());
    emit signalMessage(tr("Проект закрыт"));
}

void Core::setRyuAppPath(QString path)
{
    QSettings s("SNet", "Editor");
    s.setValue("ryu/last_app_path", path);
    emit signalMessage(tr("Ryu-приложение: %1").arg(path));
}

void Core::handleRightClickEvent(QPoint localPos, QPoint globalPos)
{
    Node *n = map.getNodeByPosition(localPos);
    if (!n) return;
    DeviceType dt = n->getDeviceType();
    if (dt == HOST)
    {
        Host *clicked = static_cast<Host *>(n);
        QList<Host *> hosts = map.getHosts();
        QStringList others;
        for (Host *h : hosts)
        {
            if (h == clicked) continue;
            others << h->getName();
        }
        emit signalHostContextRequested(clicked->getName(), others, globalPos);
        return;
    }
    if (dt == SDNCONTROLLER)
    {
        emit signalControllerContextRequested(globalPos);
        return;
    }
}

void Core::pingAll()
{
    if (map.getHostCount() < 2)
    {
        emit signalMessage(tr("Для Ping All нужно минимум 2 хоста"));
        return;
    }
    emit signalMessage(tr("Ping All → HTTP %1…").arg(mnHttp->baseUrl()));
    mnHttp->pingAll();
}

void Core::onMininetLaunched(const QString &msg)
{
    Q_UNUSED(msg);
    emit signalMininetStatusChanged(tr("работает"));
}

void Core::onMininetFailed(const QString &reason)
{
    emit signalMessage(tr("Ошибка процесса: %1").arg(reason));
    emit signalMininetStatusChanged(tr("ошибка"));
}

void Core::onPingAllReceived(const QVariantMap &result)
{
    int success = result.value("success").toInt();
    int total = result.value("total").toInt();
    emit signalPingStatsChanged(success, total);

    QVariantList matrix = result.value("matrix").toList();
    for (const QVariant &v : matrix)
    {
        QVariantMap m = v.toMap();
        QString src = m.value("src").toString();
        QString dst = m.value("dst").toString();
        double rtt = m.value("rtt_ms").toDouble();
        double loss = m.value("loss").toDouble();
        emit signalMessage(QString("  %1 → %2: rtt=%3 ms, loss=%4%%")
                           .arg(src).arg(dst)
                           .arg(rtt, 0, 'f', 2)
                           .arg(loss, 0, 'f', 0));
        /* Запускаем анимацию для пар с loss < 100% (успешных). */
        if (loss < 100.0)
        {
            emit signalAnimatePacket(src, dst, QColor("#22C55E"));
        }
    }
    emit signalMessage(tr("Ping All завершён: %1/%2 успешно").arg(success).arg(total));
}

void Core::onPingHttpFailed(const QString &reason)
{
    emit signalMessage(tr("HTTP-запрос к Mininet не удался: %1").arg(reason));

    /* Fallback: запускаем скрипт с --pingall, ловим строку SNET_PINGALL_JSON:{...}. */
    QString scriptPath = tempMininetScriptPath;
    if (scriptPath.isEmpty() || !QFileInfo::exists(scriptPath))
    {
        /* Скрипт ещё не сгенерирован — сделаем это сейчас, чтобы не блокировать
         * пользователя. */
        scriptPath = buildAndSaveTempMininetScript();
    }
    emit signalMessage(tr("Fallback: запуск %1 с --pingall (sudo) ...")
                       .arg(QFileInfo(scriptPath).fileName()));

    /* Запускаем в фоне; считываем stdout и парсим. Используем sh, чтобы sudo и pipes работали. */
    QProcess *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this, p](int, QProcess::ExitStatus) {
        QString out = QString::fromLocal8Bit(p->readAll());
        p->deleteLater();
        int marker = out.indexOf("SNET_PINGALL_JSON:");
        if (marker < 0)
        {
            emit signalMessage(tr("Fallback не дал результата: маркер SNET_PINGALL_JSON не найден"));
            return;
        }
        QString jsonStr = out.mid(marker + QString("SNET_PINGALL_JSON:").length()).trimmed();
        int endLine = jsonStr.indexOf('\n');
        if (endLine > 0) jsonStr = jsonStr.left(endLine);

        QJsonParseError perr;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError)
        {
            emit signalMessage(tr("Fallback: ошибка JSON (%1)").arg(perr.errorString()));
            return;
        }
        QVariantMap result;
        QVariantList matrix;
        int success = 0, total = 0;
        for (const QJsonValue &v : doc.object().value("matrix").toArray())
        {
            QJsonObject e = v.toObject();
            QVariantMap m;
            m["src"] = e.value("src").toString();
            m["dst"] = e.value("dst").toString();
            m["rtt_ms"] = e.value("rtt_ms").toDouble();
            m["loss"] = e.value("loss").toDouble();
            matrix << m;
            total++;
            if (e.value("loss").toDouble() < 100.0) success++;
        }
        result["matrix"] = matrix;
        result["success"] = success;
        result["total"] = total;
        onPingAllReceived(result);
    });

    /* sudo -S читает пароль из stdin — не блокирует на TTY-промпте. */
    if (!launcher->hasSudoPassword())
    {
        emit signalMessage(tr("Fallback требует пароль sudo, но он не введён."));
        p->deleteLater();
        return;
    }
    p->start("sudo", QStringList() << "-S" << "-p" << "" << "python3"
                                   << scriptPath << "--pingall");
    if (!p->waitForStarted(2000))
    {
        emit signalMessage(tr("Fallback: не удалось запустить sudo"));
        p->deleteLater();
        return;
    }
    launcher->writeSudoPassword(p);
}

void Core::setCurrentRoutingAlgorithm(QString algorithmId)
{
    if (algorithmId.isEmpty()) return;
    currentAlgoId_ = algorithmId;
    IRoutingAlgorithm *algo = AlgorithmRegistry::instance()->get(algorithmId);
    emit signalMessage(tr("Активный алгоритм маршрутизации: %1")
                       .arg(algo ? algo->displayName() : algorithmId));
    /* После смены алгоритма — пересчитываем дерево от первого свитча и
     * подсвечиваем, чтобы пользователь видел эффект. */
    if (!map.getSwitches().isEmpty())
    {
        runRoutingAlgorithm(currentAlgoId_, 0, currentMetric_);
    }
    /* Если запущена топология — отправляем в Ryu для активной маршрутизации. */
    sendActiveAlgorithmToRyu();
}

void Core::setCurrentRoutingMetric(QString metric)
{
    if (metric.isEmpty()) return;
    currentMetric_ = metric;
    /* Перерасчёт дерева с новой метрикой. */
    if (!map.getSwitches().isEmpty())
    {
        runRoutingAlgorithm(currentAlgoId_, 0, currentMetric_);
    }
    sendActiveAlgorithmToRyu();
}

void Core::sendActiveAlgorithmToRyu()
{
    /* Отправляет GET /route/activate?name=...&metric=... контроллеру Ryu.
     * Имя берём из displayName() — оно совпадает с algorithms.py::name. */
    if (!mininetRunning) return;
    IRoutingAlgorithm *algo = AlgorithmRegistry::instance()->get(currentAlgoId_);
    if (!algo) return;
    QSettings s("SNet", "Editor");
    int port = s.value("ryu/wsgi_port", 8080).toInt();
    QUrl url(QString("http://127.0.0.1:%1/route/activate").arg(port));
    QUrlQuery q;
    q.addQueryItem("name", algo->displayName());
    q.addQueryItem("metric", currentMetric_);
    url.setQuery(q);

    QNetworkRequest req(url);
    QNetworkReply *reply = ryuNam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, algo]() {
        if (reply->error() == QNetworkReply::NoError)
            emit signalMessage(tr("Ryu: активирован алгоритм «%1»")
                               .arg(algo->displayName()));
        else
            emit signalMessage(tr("Ryu /route/activate не отвечает (%1) — "
                                  "контроллер ещё не запущен?")
                               .arg(reply->errorString()));
        reply->deleteLater();
    });
}

void Core::deactivateRyuRouting()
{
    QSettings s("SNet", "Editor");
    int port = s.value("ryu/wsgi_port", 8080).toInt();
    QNetworkRequest req(QUrl(QString("http://127.0.0.1:%1/route/clear").arg(port)));
    QNetworkReply *reply = ryuNam_->get(req);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void Core::pushUndoState()
{
    pushUndoSnapshot(map.createXmlDocument());
}

void Core::pushUndoSnapshot(const QString &preStateXml)
{
    undoMgr_.push(preStateXml);
    emit signalUndoRedoChanged(canUndo(), canRedo());
}

void Core::resetUndoHistory()
{
    undoMgr_.clear();
    emit signalUndoRedoChanged(false, false);
}

void Core::undo()
{
    if (!undoMgr_.canUndo()) return;
    QString restore = undoMgr_.undo(map.createXmlDocument());
    if (restore.isNull()) return;
    XmlDeserializer d;
    d.deserialize(restore, &map);
    elementEditor.handleMouseDown(QPoint(-999, -999));
    refreshNetworkMap();
    emit signalModelRestored();
    emit signalUndoRedoChanged(canUndo(), canRedo());
    emit signalMessage(tr("Отменено (undo). Доступно отмен: %1, повторов: %2")
                       .arg(undoMgr_.undoCount()).arg(undoMgr_.redoCount()));
}

void Core::redo()
{
    if (!undoMgr_.canRedo()) return;
    QString restore = undoMgr_.redo(map.createXmlDocument());
    if (restore.isNull()) return;
    XmlDeserializer d;
    d.deserialize(restore, &map);
    elementEditor.handleMouseDown(QPoint(-999, -999));
    refreshNetworkMap();
    emit signalModelRestored();
    emit signalUndoRedoChanged(canUndo(), canRedo());
    emit signalMessage(tr("Повторено (redo). Доступно отмен: %1, повторов: %2")
                       .arg(undoMgr_.undoCount()).arg(undoMgr_.redoCount()));
}

void Core::recomputeActiveRouting()
{
    if (map.getSwitches().isEmpty()) return;
    /* Перерисовываем подсветку для тех же корневых свичей, что выбраны
     * сейчас (после изменения/отказа канала маршруты пересчитываются по
     * обновлённому графу). Если ничего не подсвечено — ничего не делаем. */
    if (routeRoots_.isEmpty()) return;
    recomputeRouteHighlights();
}

QList<int> Core::computeRoutePath(QString hostFrom, QString hostTo)
{
    QList<int> empty;
    Node *src = map.getNodeByName(hostFrom);
    Node *dst = map.getNodeByName(hostTo);
    if (!src || !dst) return empty;

    /* Находим uplink-свитч для каждого хоста (первый сосед-свитч). */
    auto findUplinkSwitch = [&](Node *host) -> Switch *
    {
        for (SSLink *l : map.getSSLinks())
        {
            Node *a = l->getNode1();
            Node *b = l->getNode2();
            if (a == host && b && b->getDeviceType() == SWITCH)
                return static_cast<Switch *>(b);
            if (b == host && a && a->getDeviceType() == SWITCH)
                return static_cast<Switch *>(a);
        }
        return nullptr;
    };

    Switch *swSrc = (src->getDeviceType() == SWITCH)
                    ? static_cast<Switch *>(src)
                    : findUplinkSwitch(src);
    Switch *swDst = (dst->getDeviceType() == SWITCH)
                    ? static_cast<Switch *>(dst)
                    : findUplinkSwitch(dst);
    if (!swSrc || !swDst) return empty;

    QList<Switch *> switches = map.getSwitches();
    int idxSrc = switches.indexOf(swSrc);
    int idxDst = switches.indexOf(swDst);
    if (idxSrc < 0 || idxDst < 0) return empty;

    /* Строим Graph по выбранной метрике. */
    CommutationMatrix<SSLink> commGraph = map.getGraphMatrix();
    int n = switches.size();
    Graph w(n);
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            if (i == j) continue;
            SSLink *ln = commGraph.getLink(switches[i], switches[j]);
            if (!ln) continue;
            if (ln->isDisabled()) continue;  // отключённый канал — нет ребра
            double weight = ln->getDelay();
            if (currentMetric_ == "bandwidth")
                weight = (ln->getBandwidth() > 0.0) ? 1.0 / ln->getBandwidth() : 1e9;
            else if (currentMetric_ == "loss")
                weight = ln->getPacketLoss();
            if (weight <= 0.0) weight = 1.0;
            w.set(i, j, weight);
        }
    }

    IRoutingAlgorithm *algo = AlgorithmRegistry::instance()->get(currentAlgoId_);
    if (!algo) algo = AlgorithmRegistry::instance()->get("dijkstra");
    if (!algo) return empty;

    QList<int> idxPath = algo->computePath(w, idxSrc, idxDst);
    QList<int> groupIdPath;
    for (int idx : idxPath)
    {
        if (idx < 0 || idx >= n) continue;
        groupIdPath.append(switches[idx]->getGroupId());
    }
    return groupIdPath;
}

void Core::runRoutingAlgorithm(QString algorithmId, int rootSwitchIndex, QString metric)
{
    /* Прокси на расширенный метод с пустыми параметрами — для совместимости
     * со старыми вызовами и сигналами без QVariantMap. */
    runRoutingAlgorithmExt(algorithmId, rootSwitchIndex, metric, QVariantMap());
}

void Core::runRoutingAlgorithmExt(QString algorithmId, int rootSwitchIndex,
                                  QString metric, QVariantMap params)
{
    IRoutingAlgorithm *algo = AlgorithmRegistry::instance()->get(algorithmId);
    if (!algo)
    {
        emit signalMessage(tr("Алгоритм '%1' не найден").arg(algorithmId));
        return;
    }

    QList<Switch *> switches = map.getSwitches();
    if (switches.isEmpty())
    {
        emit signalMessage(tr("В топологии нет коммутаторов"));
        return;
    }
    /* Для сегментирования root не используется — поэтому проверяем его
     * только для tree-/path-алгоритмов. */
    if (algo->kind() != "segments")
    {
        if (rootSwitchIndex < 0 || rootSwitchIndex >= switches.size())
        {
            emit signalMessage(tr("Некорректный индекс корневого коммутатора"));
            return;
        }
    }

    /* Построение Graph из CommutationMatrix<SSLink>. */
    CommutationMatrix<SSLink> commGraph = map.getGraphMatrix();
    int n = switches.size();
    Graph w(n);
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            if (i == j) continue;
            SSLink *ln = commGraph.getLink(switches[i], switches[j]);
            if (!ln) continue;
            if (ln->isDisabled()) continue;  // отключённый канал — нет ребра
            double weight = 0.0;
            if (metric == "delay")
                weight = ln->getDelay();
            else if (metric == "bandwidth")
                weight = (ln->getBandwidth() > 0.0) ? 1.0 / ln->getBandwidth() : 1e9;
            else if (metric == "loss")
                weight = ln->getPacketLoss();
            else
                weight = ln->getDelay();
            if (weight <= 0.0) weight = 1.0;
            w.set(i, j, weight);
        }
    }

    QElapsedTimer timer;
    timer.start();

    /* По виду алгоритма: tree → подсвечиваем рёбра, segments → подсвечиваем узлы. */
    if (algo->kind() == "segments")
    {
        /* Число сегментов: берём из params["k"] если задано, иначе
         * умолчание 2..4 в зависимости от размера топологии. */
        int kDefault = qMax(2, qMin(int(switches.size()), 4));
        int k = params.value("k", kDefault).toInt();
        if (k < 2) k = 2;
        if (k > switches.size()) k = switches.size();
        QList<QList<int>> segIdx = algo->computeSegments(w, k);
        qint64 elapsed = timer.elapsed();

        /* Преобразуем индексы → groupId. */
        QList<QList<int>> segGid;
        for (const QList<int> &seg : segIdx)
        {
            QList<int> ids;
            for (int i : seg)
            {
                if (i >= 0 && i < n) ids << switches[i]->getGroupId();
            }
            if (!ids.isEmpty()) segGid.append(ids);
        }
        emit signalClearHighlights();
        emit signalHighlightSegments(segGid);
        emit signalAlgorithmFinished(algo->displayName(), elapsed);
        emit signalMessage(tr("Алгоритм '%1': %2 сегмент(а/ов) за %3 мс")
                           .arg(algo->displayName())
                           .arg(segGid.size())
                           .arg(elapsed));
        return;
    }

    qint64 elapsed = timer.elapsed();
    Q_UNUSED(rootSwitchIndex);

    /* Новая модель подсветки маршрутов: запоминаем алгоритм/метрику и по
     * умолчанию показываем дерево маршрутов от ПЕРВОГО свича (s1). Дальше
     * пользователь кликами по свичам добавляет/убирает деревья от них
     * (каждый свич — свой цвет; на общих рёбрах побеждает последний). */
    currentAlgoId_ = algorithmId;
    currentMetric_ = metric;
    currentMetric2_ = params.value("metric2", "loss").toString();
    currentR2_ = params.value("restriction2", 0.0).toDouble();
    currentLambda_ = params.value("lambd", 0.0).toDouble();
    routeRoots_.clear();
    routeColorIdx_.clear();
    routeNextColor_ = 0;
    if (!switches.isEmpty())
    {
        int gid0 = switches.first()->getGroupId();
        routeRoots_.append(gid0);
        routeColorIdx_[gid0] = routeNextColor_++;
    }
    recomputeRouteHighlights();
    emit signalAlgorithmFinished(algo->displayName(), elapsed);
    emit signalMessage(tr("Алгоритм '%1' выполнен за %2 мс. Маршруты от %3. "
                          "ПКМ по свичу → «Подсветить маршруты», чтобы добавить.")
                       .arg(algo->displayName())
                       .arg(elapsed)
                       .arg(switches.isEmpty() ? QString("—") : switches.first()->getName()));
}

void Core::recomputeRouteHighlights()
{
    static const QColor kPalette[] = {
        QColor("#DC2626"), QColor("#2563EB"), QColor("#16A34A"), QColor("#D97706"),
        QColor("#7C3AED"), QColor("#0891B2"), QColor("#DB2777"), QColor("#CA8A04"),
    };
    const int kPaletteN = int(sizeof(kPalette) / sizeof(kPalette[0]));

    QList<Switch *> switches = map.getSwitches();
    int n = switches.size();
    if (routeRoots_.isEmpty() || n == 0)
    {
        emit signalClearHighlights();
        return;
    }

    IRoutingAlgorithm *algo = AlgorithmRegistry::instance()->get(currentAlgoId_);
    if (!algo || algo->kind() == "segments")
        algo = AlgorithmRegistry::instance()->get("dijkstra");
    if (!algo)
    {
        emit signalClearHighlights();
        return;
    }

    /* Граф по текущей метрике (отключённые каналы исключаем). */
    CommutationMatrix<SSLink> commGraph = map.getGraphMatrix();
    Graph w(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            if (i == j) continue;
            SSLink *ln = commGraph.getLink(switches[i], switches[j]);
            if (!ln || ln->isDisabled()) continue;
            double weight;
            if (currentMetric_ == "bandwidth")
                weight = (ln->getBandwidth() > 0.0) ? 1.0 / ln->getBandwidth() : 1e9;
            else if (currentMetric_ == "loss")
                weight = ln->getPacketLoss();
            else
                weight = ln->getDelay();
            if (weight <= 0.0) weight = 1.0;
            w.set(i, j, weight);
        }

    QHash<int, int> idxOfGid;
    for (int i = 0; i < n; ++i) idxOfGid[switches[i]->getGroupId()] = i;

    /* Для LARAC строим вторую матрицу (метрика-ограничение). Веса берём «как
     * есть», БЕЗ замены 0→1: иначе чистые каналы (loss 0) считались бы за 1 и
     * порог R₂ работал бы неверно. Отсутствующие рёбра остаются INF. */
    LARACAlgorithm *larac = dynamic_cast<LARACAlgorithm *>(algo);
    Graph constr(n);
    if (larac)
    {
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
            {
                if (i == j) continue;
                SSLink *ln = commGraph.getLink(switches[i], switches[j]);
                if (!ln || ln->isDisabled()) continue;
                double v;
                if (currentMetric2_ == "bandwidth")
                    v = (ln->getBandwidth() > 0.0) ? 1.0 / ln->getBandwidth() : 1e9;
                else if (currentMetric2_ == "delay")
                    v = ln->getDelay();
                else
                    v = ln->getPacketLoss();
                constr.set(i, j, v);
            }
    }

    QList<QList<QList<int>>> trees;
    QList<QColor> colors;
    for (int gid : routeRoots_)
    {
        if (!idxOfGid.contains(gid)) continue;
        QList<QList<int>> tree = larac
            ? larac->computeConstrainedTree(w, constr, idxOfGid.value(gid),
                                            currentR2_, currentLambda_)
            : algo->computeTree(w, idxOfGid.value(gid));
        QList<QList<int>> named;
        for (const QList<int> &e : tree)
        {
            if (e.size() != 2) continue;
            int a = e[0], b = e[1];
            if (a < 0 || a >= n || b < 0 || b >= n) continue;
            named.append({switches[a]->getGroupId(), switches[b]->getGroupId()});
        }
        trees.append(named);
        colors.append(kPalette[routeColorIdx_.value(gid, 0) % kPaletteN]);
    }
    emit signalHighlightTrees(trees, colors);
}

void Core::highlightRoutesFromSwitch(QString switchName)
{
    Switch *sw = dynamic_cast<Switch *>(map.getNodeByName(switchName));
    if (!sw) return;
    int gid = sw->getGroupId();
    bool nowOn;
    if (routeRoots_.contains(gid))
    {
        routeRoots_.removeAll(gid);   // повторный клик — убрать
        nowOn = false;
    }
    else
    {
        if (!routeColorIdx_.contains(gid)) routeColorIdx_[gid] = routeNextColor_++;
        routeRoots_.append(gid);      // включить (поверх остальных)
        nowOn = true;
    }
    recomputeRouteHighlights();
    emit signalMessage(nowOn
        ? tr("Маршруты от %1 показаны").arg(switchName)
        : tr("Маршруты от %1 скрыты").arg(switchName));
}

bool Core::isSwitchRouteHighlighted(QString switchName) const
{
    Switch *sw = dynamic_cast<Switch *>(
        const_cast<NetworkMap &>(map).getNodeByName(switchName));
    if (!sw) return false;
    return routeRoots_.contains(sw->getGroupId());
}
