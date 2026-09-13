#ifndef LINKITEM_H
#define LINKITEM_H

#include <QGraphicsObject>
#include <QPointer>
#include <QColor>
#include <QString>

class Link;
class NodeItem;

/* LinkItem — линия между двумя NodeItem. Может представлять SSLink
 * (свитч-свитч / хост-свитч) или CSLink (контроллер-свитч).
 *
 * Унаследован от QGraphicsObject (QObject + QGraphicsItem), чтобы
 * допускать QPointer<LinkItem> — это спасает от dangling pointer'ов
 * при удалении узлов в QGraphicsScene::clear().
 *
 * Геометрия пересчитывается автоматически: координаты берутся из
 * текущих pos() узлов. NodeItem уведомляет LinkItem через update(). */
class LinkItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum LinkKind {
        KindSSLink = 0,   // обычный канал
        KindCSLink = 1,   // controller-switch (пунктир, без подписи)
    };

    LinkItem(NodeItem *a, NodeItem *b, Link *domainLink, LinkKind kind);
    ~LinkItem();

    NodeItem *nodeA() const;
    NodeItem *nodeB() const;
    Link *domainLink() const { return link_; }
    LinkKind kind() const { return kind_; }

    void setHighlightColor(const QColor &c);
    void clearHighlight();
    /* Помечает линк как межсегментный (для отрисовки пунктиром +
     * увеличенной толщины при сегментировании). */
    void setInterSegment(bool on) { interSegment_ = on; update(); }

    /* Текст метрики на середине (delay / bandwidth / loss). Если пусто — не рисуется. */
    void setMetricLabel(const QString &text) { metricLabel_ = text; update(); }

    /* Подписи номеров портов у концов линка (со стороны nodeA и nodeB).
     * Если обе пусты — не рисуются. Задаются TopologyScene при включении
     * соответствующей опции в меню «Вид».
     * prepareGeometryChange() обязателен, потому что boundingRect()
     * расширяется для размещения меток. */
    void setPortLabels(const QString &nearA, const QString &nearB)
    {
        prepareGeometryChange();
        portLabelA_ = nearA;
        portLabelB_ = nearB;
        update();
    }
    void clearPortLabels()
    {
        prepareGeometryChange();
        portLabelA_.clear();
        portLabelB_.clear();
        update();
    }

    /* Возвращает середину линии в координатах сцены — для анимации потока. */
    QPointF midScenePoint() const;

    /* QGraphicsItem */
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

signals:
    void doubleClicked(LinkItem *self);
    void contextMenuRequested(LinkItem *self, QPoint globalPos);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    /* QPointer — обнуляется автоматически при разрушении NodeItem.
     * Это критично: при QGraphicsScene::clear() узлы могут быть удалены
     * раньше линков, и обращение к dangling pointer вызовет crash. */
    QPointer<NodeItem> a_;
    QPointer<NodeItem> b_;
    Link *link_;
    LinkKind kind_;
    QColor highlight_;
    bool interSegment_ = false;
    QString metricLabel_;
    QString portLabelA_;   // подпись у nodeA (например "(1)")
    QString portLabelB_;   // подпись у nodeB
};

#endif // LINKITEM_H
