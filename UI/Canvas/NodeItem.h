#ifndef NODEITEM_H
#define NODEITEM_H

#include <QGraphicsObject>
#include <QPointer>
#include <QRectF>
#include <QPointF>
#include <QString>

class Node;
class LinkItem;
class TopologyScene;

/* NodeItem — графический элемент узла (Host / Switch / SDN-контроллер /
 * TextLabel) в QGraphicsScene.
 *
 * Хранит указатель на доменный объект Node (из NetworkMap). При движении
 * пользователем сразу синхронизирует position обратно в Node.
 *
 * Связанные LinkItem'ы перерисовываются автоматически — сцена их обновляет
 * через registerLink(). */
class NodeItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum NodeKind {
        KindHost = 0,
        KindSwitch = 1,
        KindController = 2,
        KindText = 3,
        KindDocker = 4,
    };

    NodeItem(Node *domainNode, NodeKind kind, QGraphicsItem *parent = nullptr);
    ~NodeItem();

    Node *domainNode() const { return node_; }
    NodeKind kind() const { return kind_; }

    /* Список линков, к которым прикреплён этот узел; используется при
     * перемещении для апдейта геометрии. */
    void registerLink(LinkItem *link);
    void unregisterLink(LinkItem *link);

    QString displayName() const;

    /* Подсветка: для алгоритмов («в дереве», «в сегменте») и для ping. */
    void setHighlightColor(const QColor &c);
    void clearHighlight();

    /* QGraphicsItem interface */
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

signals:
    void positionChanged(NodeItem *self, QPointF newPos);
    void doubleClicked(NodeItem *self);
    void contextMenuRequested(NodeItem *self, QPoint globalPos);
    void linkSourceRequested(NodeItem *self);   // для rubber-band

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    Node *node_;          // не владеет
    NodeKind kind_;
    QList<QPointer<LinkItem>> links_;   // QPointer защищает от dangling-ссылок
    QColor highlight_;    // invalid = нет
    bool hovered_;

    QSize iconSize_;
    void drawHost(QPainter *p);
    void drawSwitch(QPainter *p);
    void drawController(QPainter *p);
    void drawText(QPainter *p);
    void drawDocker(QPainter *p);
    void drawLabel(QPainter *p);
};

#endif // NODEITEM_H
