#ifndef TOPOLOGYSCENE_H
#define TOPOLOGYSCENE_H

#include <QGraphicsScene>
#include <QHash>
#include <QString>
#include <QPointF>

class Core;
class Node;
class Link;
class NodeItem;
class LinkItem;
class QGraphicsLineItem;
class PacketFlowAnimator;

/* TopologyScene — сцена, отображающая NetworkMap.
 *
 * Источник правды — NetworkMap (через Core::getMap()). Scene хранит таблицу
 * Node* → NodeItem* и Link* → LinkItem*. При rebuild() сцена пересинхронизируется
 * с моделью (используется, например, после загрузки .sdn.xml).
 *
 * Инструменты (active tool, выбирается в палитре):
 *   "select" — обычное выделение/перемещение/rubber-band (по умолчанию).
 *   "host"|"switch"|"controller" — клик по пустому создаёт узел.
 *   "link" — клик по первому узлу, потом по второму создаёт канал.
 *   "delete" — клик по узлу/линку удаляет его (как ластик).
 */
class TopologyScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit TopologyScene(Core *core, QObject *parent = nullptr);

    /* Полностью пересобрать сцену из NetworkMap. */
    void rebuildFromModel();

    /* Создать новый узел по drag-n-drop или по клику в режиме создания.
     * kind: "host", "switch", "controller". */
    void createNodeAt(const QString &kind, QPointF scenePos);

    /* Начать создание линка: показать резиновую линию от узла. */
    void beginLinkCreation(NodeItem *source);
    bool isLinkCreationActive() const { return rubberSource_ != nullptr; }
    void cancelLinkCreation();

    /* Какая метрика отображается на подписях линков: "delay"/"bandwidth"/"loss". */
    void setMetric(const QString &metric);

    /* Показывать ли номера портов у концов SSLink. Состояние хранится в сцене —
     * применяется ко всем существующим LinkItem и к новым после rebuildFromModel. */
    void setShowPorts(bool on);
    bool showPorts() const { return showPorts_; }

    NodeItem *findItem(Node *n) const;
    LinkItem *findItem(Link *l) const;

    /* Установить активный инструмент. Поддерживаются:
     * "select" (default), "host", "switch", "controller", "link", "delete". */
    void setActiveTool(const QString &tool);
    QString activeTool() const { return activeTool_; }

    /* Удалить указанные элементы (вызывается из MainWindow / из меню). */
    void deleteSelected();

    /* Доступ к аниматору (для запуска анимации потока пакетов). */
    PacketFlowAnimator *animator() const { return animator_; }

    /* === Подсветка маршрутов алгоритмами (визуализация дерева/пути) === */
    /* Снимает любую подсветку с линков (CSLink не трогает). */
    void clearAllHighlights();
    /* Подсветить дерево/путь по парам индексов свитчей (1-based groupId). */
    void highlightTreeByGroupId(const QList<QList<int>> &tree,
                                QColor color = QColor("#DC2626"));
    /* Подсветить путь (последовательность groupId свитчей). */
    void highlightPathByGroupId(const QList<int> &path,
                                QColor color = QColor("#DC2626"));
    /* Подсветить несколько деревьев маршрутов сразу (от разных свичей),
     * каждое своим цветом; на общих рёбрах побеждает последнее в списке. */
    void highlightTreesByGroupId(const QList<QList<QList<int>>> &trees,
                                 const QList<QColor> &colors);
    /* Подсветить сегменты узлов: список списков groupId. Каждый сегмент
     * получает свой цвет из встроенной палитры (8 цветов, циклично). */
    void highlightSegmentsByGroupId(const QList<QList<int>> &segments);
    /* Запустить анимацию пакетов между двумя хостами через цепочку линков
     * (по подсвеченному в данный момент маршруту). */
    void animatePacketBetweenHosts(const QString &hostFrom, const QString &hostTo,
                                   QColor color = QColor("#22C55E"));

signals:
    void messageRequested(QString msg);
    void nodeDoubleClicked(NodeItem *self);
    void nodeContextMenu(NodeItem *self, QPoint globalPos);
    void linkDoubleClicked(LinkItem *self);
    void linkContextMenu(LinkItem *self, QPoint globalPos);
    void emptyContextMenu(QPoint globalPos);
    /* Перерисовка от Core должна быть запрошена после изменения данных. */
    void modelChanged();

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private slots:
    void onNodeDoubleClicked(NodeItem *n);
    void onNodeContextMenu(NodeItem *n, QPoint globalPos);
    void onLinkSourceRequested(NodeItem *src);
    void onLinkDoubleClicked(LinkItem *li);
    void onLinkContextMenu(LinkItem *li, QPoint globalPos);

private:
    Core *core_;
    QHash<Node *, NodeItem *> nodeIndex_;
    QHash<Link *, LinkItem *> linkIndex_;

    NodeItem *rubberSource_;       // если активно — узел-исток создаваемого линка
    QGraphicsLineItem *rubberLine_;

    /* Undo для перетаскивания узлов: снимок состояния на mousePress,
     * фиксируется в undo-стек на release только если что-то реально сдвинулось. */
    bool pendingMove_ = false;
    QString pendingMoveSnapshot_;

    QString currentMetric_;
    QString activeTool_;     // "select" | "host" | "switch" | "controller" | "link" | "delete"
    bool showPorts_ = false;

    /* Запомненная подсветка алгоритма — переприменяется после
     * rebuildFromModel, чтобы клик по палитре или добавление узла не
     * сбрасывали найденный маршрут / сегменты с холста (баг #6). */
    enum HighlightMode { HL_None, HL_Tree, HL_Path, HL_Segments, HL_MultiTree };
    HighlightMode lastHlMode_ = HL_None;
    QList<QList<int>> lastHlTree_;
    QList<int> lastHlPath_;
    QList<QList<int>> lastHlSegments_;
    QColor lastHlColor_;
    QList<QList<QList<int>>> lastHlTrees_;   // для HL_MultiTree
    QList<QColor> lastHlTreeColors_;

    PacketFlowAnimator *animator_;

    void addNodeItem(Node *n);
    void addLinkItem(Link *l);
    void removeNodeItem(Node *n);
    void removeLinkItem(Link *l);
    void refreshAllLinkLabels();
    void refreshPortLabelsFor(Link *l);
    QString metricLabelFor(Link *l) const;
    void tryFinishLink(NodeItem *target);
    bool tryHandleDeleteToolClick(const QPointF &scenePos);
};

#endif // TOPOLOGYSCENE_H
