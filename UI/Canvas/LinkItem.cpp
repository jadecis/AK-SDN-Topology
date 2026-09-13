#include "LinkItem.h"
#include "NodeItem.h"
#include "ThemeManager.h"
#include "Link.h"
#include "SSLink.h"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QFont>
#include <QFontMetrics>
#include <QtMath>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>

LinkItem::LinkItem(NodeItem *a, NodeItem *b, Link *domainLink, LinkKind kind) :
    a_(a), b_(b), link_(domainLink), kind_(kind)
{
    setZValue(1);  /* под узлами */
    /* Линк selectable, чтобы Delete / rubber-band его подхватывали. */
    setFlag(ItemIsSelectable, true);
    if (a_) a_->registerLink(this);
    if (b_) b_->registerLink(this);
}

LinkItem::~LinkItem()
{
    /* QPointer обнуляется автоматически если NodeItem удалён.
     * Проверка через if (a_) — safe. */
    if (a_) a_->unregisterLink(this);
    if (b_) b_->unregisterLink(this);
}

NodeItem *LinkItem::nodeA() const { return a_.data(); }
NodeItem *LinkItem::nodeB() const { return b_.data(); }

QPointF LinkItem::midScenePoint() const
{
    if (!a_ || !b_) return QPointF();
    return (a_->pos() + b_->pos()) / 2.0;
}

void LinkItem::setHighlightColor(const QColor &c)
{
    highlight_ = c;
    update();
}

void LinkItem::clearHighlight()
{
    highlight_ = QColor();
    interSegment_ = false;
    update();
}

QRectF LinkItem::boundingRect() const
{
    if (!a_ || !b_) return QRectF();
    QPointF p1 = a_->pos();
    QPointF p2 = b_->pos();
    /* 70 px запас: подписи портов рисуются до 50 px от центра вдоль линии
     * + 14 px перпендикулярно + полширины бокса. Без этого запаса метки
     * клипаются при определённых углах наклона. */
    const qreal pad = (!portLabelA_.isEmpty() || !portLabelB_.isEmpty()) ? 70.0 : 12.0;
    qreal x1 = qMin(p1.x(), p2.x()) - pad;
    qreal y1 = qMin(p1.y(), p2.y()) - pad;
    qreal x2 = qMax(p1.x(), p2.x()) + pad;
    qreal y2 = qMax(p1.y(), p2.y()) + pad;
    return QRectF(x1, y1, x2 - x1, y2 - y1);
}

QPainterPath LinkItem::shape() const
{
    QPainterPath path;
    if (!a_ || !b_) return path;
    path.moveTo(a_->pos());
    path.lineTo(b_->pos());
    QPainterPathStroker stroker;
    stroker.setWidth(10);  /* удобный hit-test для клика */
    return stroker.createStroke(path);
}

void LinkItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!a_ || !b_) return;
    p->setRenderHint(QPainter::Antialiasing, true);
    QPointF p1 = a_->pos();
    QPointF p2 = b_->pos();

    /* Отключённый канал (симуляция отказа) рисуется серым пунктиром с
     * крестом — поверх любой подсветки/выделения. */
    SSLink *ssl = (kind_ == KindSSLink) ? dynamic_cast<SSLink *>(link_) : nullptr;
    const bool disabled = ssl && ssl->isDisabled();
    if (disabled)
    {
        p->setPen(QPen(QColor("#9CA3AF"), 2.4, Qt::DashLine, Qt::RoundCap));
        p->drawLine(p1, p2);
        /* Красный крестик на середине. */
        QPointF mid((p1.x() + p2.x()) / 2.0, (p1.y() + p2.y()) / 2.0);
        const qreal cs = 7.0;
        p->setPen(QPen(QColor("#EF4444"), 2.6, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(mid + QPointF(-cs, -cs), mid + QPointF(cs, cs));
        p->drawLine(mid + QPointF(-cs, cs), mid + QPointF(cs, -cs));
        return;
    }

    /* Приоритет цвета: highlight (от алгоритмов) > selection > обычный. */
    QColor base;
    qreal width;
    Qt::PenStyle style = (kind_ == KindCSLink) ? Qt::DashLine : Qt::SolidLine;
    if (highlight_.isValid())
    {
        base = highlight_;
        width = interSegment_ ? 4.5 : 3.5;
        if (interSegment_) style = Qt::DashLine;  // пунктир — это «мост» между сегментами
    }
    else if (isSelected())
    {
        base = QColor("#2D6CDF");
        width = 3.2;
    }
    else
    {
        base = (kind_ == KindCSLink) ? QColor("#A78BFA") : QColor("#475569");
        width = 2.0;
    }

    p->setPen(QPen(base, width, style, Qt::RoundCap));
    p->drawLine(p1, p2);

    /* Подпись метрики посередине */
    if (kind_ == KindSSLink && !metricLabel_.isEmpty())
    {
        QPointF mid((p1.x() + p2.x()) / 2.0, (p1.y() + p2.y()) / 2.0);
        QFont f("sans-serif", 8, QFont::Bold);
        p->setFont(f);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(metricLabel_) + 10;
        int th = fm.height() + 2;
        QRectF box(mid.x() - tw / 2.0, mid.y() - th / 2.0, tw, th);
        p->setBrush(QColor(255, 255, 255, 230));
        p->setPen(QPen(QColor("#E5E7EB"), 1));
        p->drawRoundedRect(box, 4, 4);
        p->setPen(QColor("#111827"));
        p->drawText(box, Qt::AlignCenter, metricLabel_);
    }

    /* Подписи номеров портов у концов линка. Только для SSLink — у CSLink
     * (контроллер↔коммутатор) их рисовать не нужно.
     *
     * Позиционирование:
     *   - Подпись отступает от центра узла вдоль линии на kAlong, но не
     *     дальше середины (для коротких линков). 50 px — чтобы не залезать
     *     на крупные иконки коммутаторов (~64x40).
     *   - И на kPerp перпендикулярно линии, чтобы НЕ перекрывать саму
     *     линию и не сталкиваться с меткой метрики посередине.
     *   - Подписи у обоих концов смещены в ОДНУ сторону от линии (по знаку
     *     нормали +n), чтобы было визуально единообразно. */
    if (kind_ == KindSSLink && (!portLabelA_.isEmpty() || !portLabelB_.isEmpty()))
    {
        QFont fp("sans-serif", 8, QFont::Bold);
        p->setFont(fp);
        QFontMetrics fmp(fp);

        QPointF dir = p2 - p1;
        qreal len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
        if (len > 1e-3)
        {
            QPointF u = dir / len;
            QPointF n(-u.y(), u.x());
            const qreal kAlong = qMin<qreal>(50.0, len * 0.35);
            const qreal kPerp  = 14.0;
            auto drawLabel = [&](const QPointF &pos, const QString &text) {
                if (text.isEmpty()) return;
                int tw = fmp.horizontalAdvance(text) + 8;
                int th = fmp.height() + 4;
                QRectF box(pos.x() - tw / 2.0, pos.y() - th / 2.0, tw, th);
                p->setBrush(QColor(255, 255, 255, 240));
                p->setPen(QPen(QColor("#94A3B8"), 1));
                p->drawRoundedRect(box, 4, 4);
                p->setPen(QColor("#0F172A"));
                p->drawText(box, Qt::AlignCenter, text);
            };
            drawLabel(p1 + u * kAlong + n * kPerp, portLabelA_);
            drawLabel(p2 - u * kAlong + n * kPerp, portLabelB_);
        }
    }
}

void LinkItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    emit doubleClicked(this);
    event->accept();
}

void LinkItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    emit contextMenuRequested(this, event->screenPos());
    event->accept();
}
