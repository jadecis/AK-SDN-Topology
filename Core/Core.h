/* Core:
 * - monitores user operations
 */

#ifndef CORE_H
#define CORE_H

#include <QObject>
#include <QColor>
#include <QList>
#include <QHash>
#include <QVariantMap>
#include "NetworkMap.h"
#include "ElementEditor.h"
#include "NodeCreator.h"
#include "LinkCreator.h"
#include "UndoCommands.h"

class Tool;
class ProcessLauncher;
class MininetHttpClient;
class ProjectManager;
class QNetworkAccessManager;

class Core : public QObject
{
    Q_OBJECT

public:
    explicit Core(QObject *parent = 0);
    ~Core();

    void refreshNetworkMap();
    NetworkMap *getMap();
    MininetHttpClient *getHttpClient() { return mnHttp; }
    ProcessLauncher *getLauncher() { return launcher; }

private:
    NetworkMap map;

    Tool *tool;
    NodeCreator nodeCreator;
    LinkCreator linkCreator;
    ElementEditor elementEditor;

    ProcessLauncher *launcher;
    MininetHttpClient *mnHttp;
    ProjectManager *project;
    QNetworkAccessManager *ryuNam_;
    QString tempMininetScriptPath;
    quint16 mininetHttpPort;
    bool mininetRunning;
    QString currentAlgoId_;
    QString currentMetric_;
    /* Параметры LARAC (2-я метрика-ограничение, порог R₂, множитель λ).
     * Запоминаются при запуске, чтобы пересчёт маршрутов при клике по свичу
     * и при отказе канала использовал те же ограничения. */
    QString currentMetric2_ = QStringLiteral("loss");
    double currentR2_ = 0.0;
    double currentLambda_ = 0.0;
    UndoManager undoMgr_;

    /* Подсветка маршрутов: список корневых свичей (groupId) в порядке
     * включения (последний — поверх остальных), стабильный цвет на свич. */
    QList<int> routeRoots_;
    QHash<int, int> routeColorIdx_;
    int routeNextColor_ = 0;
    void recomputeRouteHighlights();

signals:
    void signalRefreshNetworkMapView(QPixmap);
    void signalStatsChanged(int hosts, int switches, int links, int controllers);
    void signalMessage(QString message);
    void signalMininetStatusChanged(QString status);
    void signalPingStatsChanged(int success, int total);
    void signalAlgorithmFinished(QString algorithmName, qint64 elapsedMs);
    /* Подсветка маршрутов: tree/path в виде пар groupId свитчей. Используется
     * новой канвой (TopologyScene::highlightTreeByGroupId). */
    void signalHighlightTree(QList<QList<int>> tree, QColor color);
    void signalHighlightPath(QList<int> path, QColor color);
    void signalHighlightSegments(QList<QList<int>> segments);
    /* Несколько деревьев маршрутов сразу (от разных корневых свичей),
     * каждое своим цветом. Рисуются по порядку — последнее «побеждает»
     * на общих рёбрах. */
    void signalHighlightTrees(QList<QList<QList<int>>> trees, QList<QColor> colors);
    void signalClearHighlights();
    /* Запуск анимации потока пакетов между двумя хостами. */
    void signalAnimatePacket(QString hostFrom, QString hostTo, QColor color);
    /* Правый клик пришёлся в Host — UI должен открыть контекстное меню. */
    void signalHostContextRequested(QString hostName, QStringList otherHostNames, QPoint globalPos);
    /* Правый клик в SDN-контроллер. */
    void signalControllerContextRequested(QPoint globalPos);
    /* Состояние проекта изменилось (новый/открыт/сохранён/закрыт). */
    void signalProjectChanged(QString dir, QString name);
    /* Модель восстановлена из снимка undo/redo — сцену нужно пересобрать. */
    void signalModelRestored();
    /* Доступность undo/redo изменилась — обновить состояние кнопок/меню. */
    void signalUndoRedoChanged(bool canUndo, bool canRedo);

public slots:
    void connectSdnController();
    void clearNetworkMap();

    void handleMouseReleaseEvent(QPoint);
    void handleMousePressEvent(QPoint);
    void handleMouseMoveEvent(QPoint);
    void handleDoubleClickEvent(QPoint);
    void handleKeyDeletePressEvent();

    void changeStateToEdit();
    void prepareSdnController();
    void prepareHost();
    void prepareSwitch();
    void prepareLink();
    void prepareTextLabel();

    void showPorts(bool);
    void showBandwidth();
    void showDelay();
    void showPacketLossRate();

    void createMininetScript(QString);
    void createWeightsMatrix(QString path);

    void saveNetworkMap(QString);
    void loadNetworkMap(QString);

    void visualizePath(QList<int> path);
    void visualizePaths(QList<QList<int> > paths);
    void visualizeIslands(QList<QList<int> > islands);
    void visualizeTree(QList<QList<int> > tree);
    void visualizeTrees(QList<QList<QList<int> > > trees);
    void eraseMarks();

    void changeMetric(QVector<float> metricData);

    void AlignVertically();
    void AlignHorizontally();

    /* Запуск внешних процессов */
    void launchMininet();
    void launchRyu();
    /* Запустить и Mininet и Ryu одновременно (для кнопки «Старт топологии»). */
    void startTopology();
    /* Остановить Mininet (HTTP /shutdown + sudo mn -c). */
    void stopTopology();
    /* Открыть xterm для хоста через HTTP-эндпойнт. */
    void openHostXterm(QString hostName);
    /* Открыть controller.py в системном редакторе (xdg-open). */
    void openControllerCode();
    /* Открыть веб-интерфейс Ryu контроллера в браузере (http://localhost:8080). */
    void openControllerWebUI();
    /* Создать пользовательский алгоритм по шаблону и открыть в редакторе. */
    void createUserAlgorithm(QString fileName, QString className, QString algoName);
    /* Открыть папку с пользовательскими алгоритмами в файловом менеджере. */
    void openUserAlgorithmsFolder();

    /* Указать пользователю Ryu .py файл (через диалог в UI) и сохранить путь */
    void setRyuAppPath(QString path);

    /* Ping All — пытается через HTTP, иначе fallback на --pingall */
    void pingAll();

    /* Обработать правый клик на канве: если попали в Host — emit
     * signalHostContextRequested. */
    void handleRightClickEvent(QPoint localPos, QPoint globalPos);

    /* Project management */
    void newProjectFromTemplate(QString projectDir, QString name, int tmplInt);
    void openProjectFromDir(QString projectDir);
    void saveProjectAll();
    void closeCurrentProject();
    ProjectManager *getProject() { return project; }

    /* Выполнить алгоритм маршрутизации (по имени из AlgorithmRegistry) */
    void runRoutingAlgorithm(QString algorithmId, int rootSwitchIndex, QString metric);
    /* То же, но с дополнительными параметрами (K для сегментирования,
     * ограничения для LARAC). При пустом params поведение совпадает с
     * базовой версией. */
    void runRoutingAlgorithmExt(QString algorithmId, int rootSwitchIndex,
                                QString metric, QVariantMap params);

    /* === Активный алгоритм маршрутизации для анимаций и подсветок === */
    void setCurrentRoutingAlgorithm(QString algorithmId);
    QString currentRoutingAlgorithm() const { return currentAlgoId_; }
    QString currentRoutingMetric() const { return currentMetric_; }
    void setCurrentRoutingMetric(QString metric);

    /* Вычислить маршрут (последовательность Switch groupId) между двумя
     * хостами через выбранный алгоритм + метрику. Если хост напрямую
     * не подключён к свитчу — берём первый доступный uplink. Возвращает
     * пустой список если маршрут не найден. */
    QList<int> computeRoutePath(QString hostFrom, QString hostTo);

    /* Пересчитать активный алгоритм (вызывать после изменения свойств
     * канала). Если выбранный алгоритм не требует полного пересчёта
     * (needsFullRecompute()==false), не делает ничего. */
    void recomputeActiveRouting();

    /* === Подсветка маршрутов по свичам ===
     * Переключает (вкл/выкл) подсветку дерева маршрутов от указанного свича.
     * Каждый свич — свой цвет, накапливается; на общих рёбрах побеждает
     * последний включённый. Использует текущий алгоритм/метрику. */
    void highlightRoutesFromSwitch(QString switchName);
    /* Подсвечены ли сейчас маршруты от данного свича (для подписи меню). */
    bool isSwitchRouteHighlighted(QString switchName) const;

    /* === Undo/Redo (#6) ===
     * pushUndoState() вызывается ПЕРЕД любым изменением модели (узлы/каналы/
     * свойства). pushUndoSnapshot() — для случаев, где состояние «до» было
     * захвачено заранее (перемещение узла). undo()/redo() восстанавливают
     * модель и эмитят signalModelRestored() для пересборки сцены. */
    void pushUndoState();
    void pushUndoSnapshot(const QString &preStateXml);
    void resetUndoHistory();
    bool canUndo() const { return undoMgr_.canUndo(); }
    bool canRedo() const { return undoMgr_.canRedo(); }
    QString captureStateXml() { return map.createXmlDocument(); }
    void undo();
    void redo();

private slots:
    void onMininetLaunched(const QString &msg);
    void onMininetFailed(const QString &reason);
    void onMininetCleanupFinished();
    void onPingAllReceived(const QVariantMap &result);
    void onPingHttpFailed(const QString &reason);

private:
    QString buildAndSaveTempMininetScript();
    void emitStats();
    /* Спрашивает пароль sudo (QInputDialog::Password) и проверяет его.
     * Возвращает true если пароль введён и принят. */
    bool ensureSudoPassword();
    /* Отправляет активный алгоритм + метрику в Ryu (GET /route/activate). */
    void sendActiveAlgorithmToRyu();
    /* GET /route/clear — выключить активную маршрутизацию. */
    void deactivateRyuRouting();
};

#endif // CORE_H
