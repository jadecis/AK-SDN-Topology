#include "NodeItem.h"
#include "LinkItem.h"
#include "ThemeManager.h"
#include "Node.h"
#include "TextLabel.h"
#include "DockerNode.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QApplication>
#include <QFont>
#include <QFontMetrics>

NodeItem::NodeItem(Node *domainNode, NodeKind kind, QGraphicsItem *parent) :
    QGraphicsObject(parent),
    node_(domainNode),
    kind_(kind),
    hovered_(false)
{
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setCursor(Qt::OpenHandCursor);
    setZValue(10);

    /* Размер базового изображения. Швитч шире, остальные квадратные. */
    if (kind == KindSwitch)
        iconSize_ = QSize(72, 36);
    else if (kind == KindController)
        iconSize_ = QSize(56, 56);
    else if (kind == KindHost)
        iconSize_ = QSize(56, 48);
    else if (kind == KindDocker)
        iconSize_ = QSize(60, 50);
    else
        iconSize_ = QSize(80, 30);

    /* Синхронизация позиции с доменным объектом. */
    if (node_)
    {
        QPoint p = node_->getPosition();
        setPos(p.x(), p.y());
    }
}

NodeItem::~NodeItem()
{
    /* Линки удаляют свои регистрации сами при разрушении. */
}

void NodeItem::registerLink(LinkItem *link)
{
    /* Чистим обнулённые QPointer'ы и добавляем новый, если ещё не зарегистрирован. */
    for (int i = links_.size() - 1; i >= 0; --i)
    {
        if (!links_[i]) links_.removeAt(i);
    }
    QPointer<LinkItem> p(link);
    if (!links_.contains(p)) links_.append(p);
}

void NodeItem::unregisterLink(LinkItem *link)
{
    QPointer<LinkItem> p(link);
    links_.removeAll(p);
}

QString NodeItem::displayName() const
{
    if (!node_) return QString();
    return node_->getName();
}

void NodeItem::setHighlightColor(const QColor &c)
{
    highlight_ = c;
    update();
}

void NodeItem::clearHighlight()
{
    highlight_ = QColor();
    update();
}

QRectF NodeItem::boundingRect() const
{
    /* Включаем место под подпись имени снизу и под halo-кольцо подсветки
     * сегмента (рисуется на расстоянии 11 px от иконки). */
    return QRectF(-iconSize_.width() / 2.0 - 14,
                  -iconSize_.height() / 2.0 - 14,
                  iconSize_.width() + 28,
                  iconSize_.height() + 40);
}

void NodeItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing, true);

    if (highlight_.isValid())
    {
        /* Подсветка сегмента: толстая яркая обводка + полупрозрачная заливка.
         * Раньше pen=2.4 + alpha=40 — выглядело как еле заметная линия,
         * пользователи жаловались что не видят сегменты. Теперь рамка
         * 4.5 px и плотность заливки 110/255 — сегменты сразу читаются. */
        QRectF r(-iconSize_.width() / 2.0 - 6, -iconSize_.height() / 2.0 - 6,
                 iconSize_.width() + 12, iconSize_.height() + 12);
        p->setPen(QPen(highlight_, 4.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(QColor(highlight_.red(), highlight_.green(),
                           highlight_.blue(), 110));
        p->drawRoundedRect(r, 10, 10);

        /* Внешний halo-контур — лёгкая полупрозрачная линия с зазором,
         * добавляет глубину и помогает различать соседние сегменты. */
        QRectF halo(-iconSize_.width() / 2.0 - 11, -iconSize_.height() / 2.0 - 11,
                    iconSize_.width() + 22, iconSize_.height() + 22);
        QColor haloPen(highlight_);
        haloPen.setAlpha(160);
        p->setPen(QPen(haloPen, 1.8, Qt::DotLine));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(halo, 13, 13);
    }

    if (isSelected())
    {
        QRectF r(-iconSize_.width() / 2.0 - 4, -iconSize_.height() / 2.0 - 4,
                 iconSize_.width() + 8, iconSize_.height() + 8);
        QPen pen(ThemeManager::instance()->accentColor(), 2,
                 Qt::DashLine);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(r, 6, 6);
    }
    else if (hovered_)
    {
        QRectF r(-iconSize_.width() / 2.0 - 2, -iconSize_.height() / 2.0 - 2,
                 iconSize_.width() + 4, iconSize_.height() + 4);
        p->setPen(QPen(QColor(0, 0, 0, 0)));
        p->setBrush(QColor(45, 108, 223, 25));
        p->drawRoundedRect(r, 4, 4);
    }

    switch (kind_)
    {
        case KindHost:       drawHost(p); break;
        case KindSwitch:     drawSwitch(p); break;
        case KindController: drawController(p); break;
        case KindText:       drawText(p); break;
        case KindDocker:     drawDocker(p); break;
    }

    drawLabel(p);
}

void NodeItem::drawHost(QPainter *p)
{
    QRectF r(-iconSize_.width() / 2.0, -iconSize_.height() / 2.0,
             iconSize_.width(), iconSize_.height());
    /* Корпус ПК */
    p->setBrush(QColor("#FFFFFF"));
    p->setPen(QPen(QColor("#2D6CDF"), 2));
    p->drawRoundedRect(r.adjusted(0, 0, 0, -10), 4, 4);
    /* Экран — светло-синий */
    p->setBrush(QColor("#E8F0FE"));
    p->setPen(QPen(QColor("#5BA0FF"), 1));
    p->drawRoundedRect(r.adjusted(4, 4, -4, -14), 2, 2);
    /* Подставка */
    p->setBrush(QColor("#2D6CDF"));
    p->setPen(QPen(QColor("#2D6CDF"), 0));
    QRectF stand(r.center().x() - 8, r.bottom() - 10, 16, 4);
    p->drawRoundedRect(stand, 2, 2);
    /* Метка PC */
    p->setPen(Qt::white);
    p->setFont(QFont("sans-serif", 7, QFont::Bold));
    p->drawText(r.adjusted(0, 4, 0, -14), Qt::AlignCenter, "PC");
}

void NodeItem::drawDocker(QPainter *p)
{
    QRectF r(-iconSize_.width() / 2.0, -iconSize_.height() / 2.0,
             iconSize_.width(), iconSize_.height());
    /* «Кит» — корпус, голубой Docker-style. */
    p->setBrush(QColor("#0DB7ED"));
    p->setPen(QPen(QColor("#0A6EBD"), 2));
    QRectF hull(r.left(), r.top() + r.height() * 0.45,
                r.width(), r.height() * 0.45);
    p->drawRoundedRect(hull, 8, 8);
    /* Стопка «контейнеров» сверху. */
    p->setBrush(QColor("#FFFFFF"));
    p->setPen(QPen(QColor("#0A6EBD"), 1));
    qreal boxW = r.width() / 5.0;
    qreal boxH = r.height() / 4.5;
    qreal yRow1 = r.top() + r.height() * 0.05;
    qreal yRow2 = yRow1 + boxH + 2;
    for (int i = 0; i < 4; ++i)
    {
        QRectF box(r.left() + 2 + i * (boxW + 1), yRow2, boxW - 2, boxH);
        p->drawRoundedRect(box, 1, 1);
    }
    for (int i = 0; i < 3; ++i)
    {
        QRectF box(r.left() + 4 + boxW + i * (boxW + 1), yRow1, boxW - 2, boxH);
        p->drawRoundedRect(box, 1, 1);
    }
    /* Метка */
    p->setPen(QColor("#FFFFFF"));
    p->setFont(QFont("sans-serif", 7, QFont::Bold));
    p->drawText(QRectF(r.left(), r.bottom() - 10, r.width(), 10),
                Qt::AlignCenter, "DKR");

    /* Health-индикатор (#3): цветной круг в правом верхнем углу.
     * 🟢 healthy / 🟡 starting / 🔴 dead / ⚫ unknown. Цвет берём из
     * рантайм-статуса DockerNode, обновляемого поллингом /docker/health. */
    DockerNode *dn = dynamic_cast<DockerNode *>(node_);
    if (dn)
    {
        QColor c;
        switch (dn->getRuntimeStatus())
        {
            case DockerNode::StatusHealthy:  c = QColor("#22C55E"); break;
            case DockerNode::StatusStarting: c = QColor("#F59E0B"); break;
            case DockerNode::StatusDead:     c = QColor("#EF4444"); break;
            default:                         c = QColor("#64748B"); break;
        }
        const qreal rad = 5.0;
        QPointF center(r.right() - rad, r.top() + rad);
        p->setPen(QPen(QColor("#FFFFFF"), 1.5));
        p->setBrush(c);
        p->drawEllipse(center, rad, rad);
    }
}

void NodeItem::drawSwitch(QPainter *p)
{
    QRectF r(-iconSize_.width() / 2.0, -iconSize_.height() / 2.0,
             iconSize_.width(), iconSize_.height());
    /* Корпус */
    p->setBrush(QColor("#1E4FB1"));
    p->setPen(QPen(QColor("#0F172A"), 1));
    p->drawRoundedRect(r, 4, 4);
    /* Верхняя полоса */
    p->setBrush(QColor("#2D6CDF"));
    p->setPen(Qt::NoPen);
    QRectF top(r.left(), r.top(), r.width(), 8);
    p->drawRoundedRect(top, 4, 4);
    /* Порты */
    p->setBrush(QColor("#94A3B8"));
    p->setPen(Qt::NoPen);
    int portCount = 5;
    qreal portW = 6;
    qreal startX = r.left() + 6;
    qreal stepX = (r.width() - 12) / (portCount - 1);
    qreal portY = r.bottom() - 10;
    for (int i = 0; i < portCount; ++i)
    {
        QRectF pr(startX + i * stepX - portW / 2, portY, portW, 4);
        p->setBrush(i == portCount - 1 ? QColor("#22C55E") : QColor("#94A3B8"));
        p->drawRoundedRect(pr, 1, 1);
    }
    /* Метка OPENFLOW */
    p->setPen(Qt::white);
    p->setFont(QFont("sans-serif", 6, QFont::Bold));
    QRectF labelR(r.left(), r.top() + 10, r.width(), 14);
    p->drawText(labelR, Qt::AlignCenter, "OPENFLOW");
}

void NodeItem::drawController(QPainter *p)
{
    QRectF r(-iconSize_.width() / 2.0, -iconSize_.height() / 2.0,
             iconSize_.width(), iconSize_.height());
    /* Корпус сервера */
    p->setBrush(QColor("#FFFFFF"));
    p->setPen(QPen(QColor("#7C3AED"), 2));
    p->drawRoundedRect(r, 6, 6);
    /* 3 «модуля» внутри */
    p->setBrush(QColor("#EDE9FE"));
    p->setPen(QPen(QColor("#A78BFA"), 1));
    qreal slotH = (r.height() - 12) / 3.0;
    for (int i = 0; i < 3; ++i)
    {
        QRectF slot(r.left() + 4, r.top() + 4 + i * slotH,
                    r.width() - 8, slotH - 2);
        p->drawRoundedRect(slot, 2, 2);
        /* LED-индикатор */
        p->setBrush(QColor("#22C55E"));
        p->setPen(Qt::NoPen);
        p->drawEllipse(QPointF(slot.left() + 4, slot.center().y()), 1.4, 1.4);
        p->setBrush(QColor("#EDE9FE"));
        p->setPen(QPen(QColor("#A78BFA"), 1));
    }
}

void NodeItem::drawText(QPainter *p)
{
    /* Содержимое и размер шрифта берём из TextLabel. */
    QString content;
    int pt = 11;
    if (auto tl = dynamic_cast<TextLabel *>(node_))
    {
        content = tl->getContent();
        pt = tl->getFontSize();
    }
    if (content.isEmpty()) content = displayName();
    if (content.isEmpty()) content = QStringLiteral("Текст");

    QFont font("sans-serif", pt, QFont::Medium);
    QFontMetrics fm(font);

    /* Многострочный текст: считаем размер блока по самой широкой строке
     * и сумме высот. Используем QFontMetrics::boundingRect для каждой
     * строки. Без этого drawText с одним rect и AlignCenter отображал
     * только первую строку — пользователь жаловался. */
    const QStringList lines = content.split('\n');
    int maxLineW = 0;
    for (const QString &ln : lines)
        maxLineW = qMax(maxLineW, fm.horizontalAdvance(ln));
    const int lineH = fm.height();
    const int blockH = lineH * qMax(1, lines.size()) +
                       qMax(0, lines.size() - 1) * 2;   // межстрочный интервал

    int tw = qMax(int(iconSize_.width()), maxLineW + 16);
    int th = qMax(int(iconSize_.height()), blockH + 10);

    if (iconSize_.width() != tw || iconSize_.height() != th)
    {
        prepareGeometryChange();
        const_cast<NodeItem *>(this)->iconSize_ = QSize(tw, th);
    }

    QRectF r(-tw / 2.0, -th / 2.0, tw, th);
    p->setPen(ThemeManager::instance()->iconColor());
    p->setFont(font);
    /* TextWordWrap для длинных строк + перенос строк по \n. */
    p->drawText(r, Qt::AlignCenter | Qt::TextWordWrap, content);
}

void NodeItem::drawLabel(QPainter *p)
{
    if (kind_ == KindText) return;
    QString name = displayName();
    if (name.isEmpty()) return;
    p->setPen(ThemeManager::instance()->iconColor());
    p->setFont(QFont("sans-serif", 9, QFont::Bold));
    QFontMetrics fm(p->font());
    int tw = fm.horizontalAdvance(name);
    int th = fm.height();
    QRectF r(-tw / 2.0 - 4, iconSize_.height() / 2.0 + 4, tw + 8, th + 2);
    p->setBrush(QColor(255, 255, 255, 200));
    p->setPen(Qt::NoPen);
    p->drawRoundedRect(r, 4, 4);
    p->setPen(QColor("#111827"));
    p->drawText(r, Qt::AlignCenter, name);
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged)
    {
        QPointF p = value.toPointF();
        if (node_)
        {
            node_->setPosition(QPoint(int(p.x()), int(p.y())));
        }
        emit positionChanged(this, p);
        /* Перерисовать все связанные линки. */
        for (const QPointer<LinkItem> &l : links_) {
            if (l) l->update();
        }
    }
    return QGraphicsObject::itemChange(change, value);
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    emit doubleClicked(this);
}

void NodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    emit contextMenuRequested(this, event->screenPos());
    event->accept();
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    setCursor(Qt::ClosedHandCursor);
    QGraphicsObject::mousePressEvent(event);
    /* Shift+ЛКМ — начать создание линка от этого узла. */
    if (event->button() == Qt::LeftButton &&
        (event->modifiers() & Qt::ShiftModifier))
    {
        emit linkSourceRequested(this);
    }
}

void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    hovered_ = true;
    update();
    setCursor(Qt::OpenHandCursor);
}

void NodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    hovered_ = false;
    update();
}
