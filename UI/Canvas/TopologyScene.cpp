#include "TopologyScene.h"
#include "NodeItem.h"
#include "LinkItem.h"
#include "PacketFlowAnimator.h"
#include "ThemeManager.h"
#include "Core.h"
#include "NetworkMap.h"
#include "Node.h"
#include "Host.h"
#include "Switch.h"
#include "DockerNode.h"
#include "SdnController.h"
#include "SSLink.h"
#include "CSLink.h"
#include "Link.h"
#include "TextLabel.h"

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QApplication>

TopologyScene::TopologyScene(Core *core, QObject *parent) :
    QGraphicsScene(parent),
    core_(core),
    rubberSource_(nullptr),
    rubberLine_(nullptr),
    currentMetric_("delay"),
    activeTool_("select"),
    animator_(nullptr)
{
    /* Большое логическое пространство — фактическая канва безразмерная,
     * но scene нужно зарезервировать ограниченный rect для оптимизации. */
    setSceneRect(-5000, -5000, 10000, 10000);
    /* Канва меняет фон вместе с темой — иначе на тёмной теме большое
     * белое поле бьёт в глаза. */
    auto applyBg = [this]() {
        QColor bg = (ThemeManager::instance()->currentTheme() == ThemeManager::Dark)
                    ? QColor("#0B1424") : QColor("#FFFFFF");
        setBackgroundBrush(bg);
    };
    applyBg();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this, applyBg](ThemeManager::Theme) { applyBg(); update(); });

    animator_ = new PacketFlowAnimator(this);
}

void TopologyScene::setMetric(const QString &metric)
{
    currentMetric_ = metric;
    refreshAllLinkLabels();
}

QString TopologyScene::metricLabelFor(Link *l) const
{
    SSLink *s = dynamic_cast<SSLink *>(l);
    if (!s) return QString();
    if (currentMetric_ == "bandwidth")
        return QString::number(s->getBandwidth());
    if (currentMetric_ == "loss")
        return QString::number(s->getPacketLoss()) + "%";
    return QString::number(s->getDelay()) + "ms";
}

void TopologyScene::refreshAllLinkLabels()
{
    for (auto it = linkIndex_.begin(); it != linkIndex_.end(); ++it)
    {
        it.value()->setMetricLabel(metricLabelFor(it.key()));
        refreshPortLabelsFor(it.key());
    }
}

void TopologyScene::setShowPorts(bool on)
{
    if (showPorts_ == on) return;
    showPorts_ = on;
    for (auto it = linkIndex_.begin(); it != linkIndex_.end(); ++it)
        refreshPortLabelsFor(it.key());
}

void TopologyScene::refreshPortLabelsFor(Link *l)
{
    LinkItem *li = linkIndex_.value(l, nullptr);
    if (!li) return;
    if (!showPorts_ || dynamic_cast<SSLink *>(l) == nullptr)
    {
        li->clearPortLabels();
        return;
    }
    if (!core_ || !core_->getMap()) { li->clearPortLabels(); return; }
    PortMatrix pm = core_->getMap()->getPortMatrix();
    Node *n1 = l->getNode1();
    Node *n2 = l->getNode2();
    if (!n1 || !n2) { li->clearPortLabels(); return; }
    int pa = pm.getPortNumber(n1, n2);
    int pb = pm.getPortNumber(n2, n1);
    li->setPortLabels(QString("(%0)").arg(pa), QString("(%0)").arg(pb));
}

NodeItem *TopologyScene::findItem(Node *n) const
{
    return nodeIndex_.value(n, nullptr);
}

LinkItem *TopologyScene::findItem(Link *l) const
{
    return linkIndex_.value(l, nullptr);
}

void TopologyScene::setActiveTool(const QString &tool)
{
    activeTool_ = tool.isEmpty() ? QString("select") : tool;
    cancelLinkCreation();
    if (activeTool_ == "select")
    {
        emit messageRequested(tr("Инструмент: Выделение (ЛКМ — выделить/перетащить, Ctrl+ЛКМ — рамка)"));
    }
    else if (activeTool_ == "host")
    {
        emit messageRequested(tr("Инструмент: Хост (клик по пустому месту — создать)"));
    }
    else if (activeTool_ == "switch")
    {
        emit messageRequested(tr("Инструмент: Коммутатор (клик по пустому месту — создать)"));
    }
    else if (activeTool_ == "controller")
    {
        emit messageRequested(tr("Инструмент: Контроллер (клик по пустому месту — создать)"));
    }
    else if (activeTool_ == "docker")
    {
        emit messageRequested(tr("Инструмент: Docker-контейнер (клик по пустому месту — создать)"));
    }
    else if (activeTool_ == "text")
    {
        emit messageRequested(tr("Инструмент: Текстовая метка (клик по пустому месту — создать)"));
    }
    else if (activeTool_ == "link")
    {
        emit messageRequested(tr("Инструмент: Канал. ЛКМ по первому узлу, потом по второму. ESC — отмена."));
    }
    else if (activeTool_ == "delete")
    {
        emit messageRequested(tr("Инструмент: Ластик. Клик по узлу или каналу — удалить."));
    }
}

void TopologyScene::rebuildFromModel()
{
    /* Сначала удаляем линки, потом узлы — гарантированно избегаем
     * dangling-ссылок в LinkItem::~. */
    cancelLinkCreation();
    if (animator_) animator_->clearAll();

    for (LinkItem *li : linkIndex_.values())
    {
        if (li && li->scene() == this) removeItem(li);
        delete li;
    }
    linkIndex_.clear();

    for (NodeItem *ni : nodeIndex_.values())
    {
        if (ni && ni->scene() == this) removeItem(ni);
        delete ni;
    }
    nodeIndex_.clear();

    /* На всякий случай — clear() удалит то, что могло остаться (rubberLine_,
     * частицы анимации и т.п.). Не делаем clear() полностью, потому что
     * хотим оставить аниматор работоспособным. */
    QList<QGraphicsItem *> leftover = items();
    for (QGraphicsItem *it : leftover)
    {
        /* Не удаляем NodeItem/LinkItem — они уже удалены выше. Не удаляем
         * частицы аниматора (они QGraphicsEllipseItem без специального типа). */
        if (dynamic_cast<NodeItem *>(it) || dynamic_cast<LinkItem *>(it))
        {
            removeItem(it);
            delete it;
        }
    }

    if (!core_) return;
    NetworkMap *map = core_->getMap();
    if (!map) return;

    for (Switch *sw : map->getSwitches()) addNodeItem(sw);
    for (Host *h : map->getHosts()) addNodeItem(h);
    for (DockerNode *d : map->getDockerNodes()) addNodeItem(d);
    for (SdnController *c : map->getSdnControllers()) addNodeItem(c);
    /* Текстовые метки тоже нужны на сцене — иначе они есть в модели,
     * но «невидимы» (та же проблема, что раньше была у Docker-узлов). */
    for (TextLabel *t : map->getTextLabels()) addNodeItem(t);

    for (SSLink *l : map->getSSLinks())
    {
        if (l && !linkIndex_.contains(l)) addLinkItem(l);
    }
    for (CSLink *l : map->getCSLinks())
    {
        if (l && !linkIndex_.contains(l)) addLinkItem(l);
    }

    refreshAllLinkLabels();

    /* Переприменяем подсветку алгоритма (если она была) — иначе клик
     * по палитре или добавление узла сбрасывали найденный маршрут /
     * сегменты, потому что rebuildFromModel пересоздаёт все LinkItem.
     * Делаем это с локальной копии состояния, потому что highlight*
     * методы переписывают lastHl*-переменные. */
    HighlightMode m = lastHlMode_;
    QList<QList<int>> tree = lastHlTree_;
    QList<int> path = lastHlPath_;
    QList<QList<int>> segs = lastHlSegments_;
    QColor col = lastHlColor_;
    QList<QList<QList<int>>> trees = lastHlTrees_;
    QList<QColor> treeColors = lastHlTreeColors_;
    switch (m)
    {
    case HL_Tree:      if (!tree.isEmpty()) highlightTreeByGroupId(tree, col); break;
    case HL_Path:      if (!path.isEmpty()) highlightPathByGroupId(path, col); break;
    case HL_Segments:  if (!segs.isEmpty()) highlightSegmentsByGroupId(segs); break;
    case HL_MultiTree: if (!trees.isEmpty()) highlightTreesByGroupId(trees, treeColors); break;
    case HL_None: default: break;
    }

    emit modelChanged();
}

void TopologyScene::addNodeItem(Node *n)
{
    if (!n) return;
    NodeItem::NodeKind kind = NodeItem::KindHost;
    switch (n->getDeviceType())
    {
        case HOST:          kind = NodeItem::KindHost; break;
        case SWITCH:        kind = NodeItem::KindSwitch; break;
        case SDNCONTROLLER: kind = NodeItem::KindController; break;
        case TEXTLABEL:     kind = NodeItem::KindText; break;
        case DOCKERNODE:    kind = NodeItem::KindDocker; break;
        default: return;
    }
    NodeItem *item = new NodeItem(n, kind);
    addItem(item);
    nodeIndex_[n] = item;
    connect(item, &NodeItem::doubleClicked,
            this, &TopologyScene::onNodeDoubleClicked);
    connect(item, &NodeItem::contextMenuRequested,
            this, &TopologyScene::onNodeContextMenu);
    connect(item, &NodeItem::linkSourceRequested,
            this, &TopologyScene::onLinkSourceRequested);
}

void TopologyScene::addLinkItem(Link *l)
{
    if (!l) return;
    Node *n1 = l->getNode1();
    Node *n2 = l->getNode2();
    if (!n1 || !n2) return;
    NodeItem *i1 = nodeIndex_.value(n1, nullptr);
    NodeItem *i2 = nodeIndex_.value(n2, nullptr);
    if (!i1 || !i2) return;

    LinkItem::LinkKind kind = (dynamic_cast<CSLink *>(l) != nullptr)
                              ? LinkItem::KindCSLink
                              : LinkItem::KindSSLink;
    LinkItem *li = new LinkItem(i1, i2, l, kind);
    li->setMetricLabel(metricLabelFor(l));
    addItem(li);
    linkIndex_[l] = li;
    connect(li, &LinkItem::doubleClicked,
            this, &TopologyScene::onLinkDoubleClicked);
    connect(li, &LinkItem::contextMenuRequested,
            this, &TopologyScene::onLinkContextMenu);
}

void TopologyScene::removeNodeItem(Node *n)
{
    NodeItem *item = nodeIndex_.take(n);
    if (!item) return;
    removeItem(item);
    delete item;
}

void TopologyScene::removeLinkItem(Link *l)
{
    LinkItem *li = linkIndex_.take(l);
    if (!li) return;
    removeItem(li);
    delete li;
}

void TopologyScene::createNodeAt(const QString &kind, QPointF scenePos)
{
    if (!core_) return;
    NetworkMap *map = core_->getMap();
    if (!map) return;
    QPoint p(int(scenePos.x()), int(scenePos.y()));
    if (kind == "host")
    {
        core_->pushUndoState();
        Host *h = new Host(p);
        map->addHost(h);
    }
    else if (kind == "switch")
    {
        core_->pushUndoState();
        Switch *s = new Switch(p);
        map->addSwitch(s);
    }
    else if (kind == "controller")
    {
        core_->pushUndoState();
        SdnController *c = new SdnController(p);
        map->addSdnController(c);
    }
    else if (kind == "docker")
    {
        core_->pushUndoState();
        DockerNode *d = new DockerNode(p);
        map->addDockerNode(d);
    }
    else if (kind == "text")
    {
        /* Создаём временно без quickConfig и спрашиваем у пользователя
         * текст явно. Если он нажал «Отмена» — удаляем объект, чтобы на
         * канве не появлялась пустая дефолтная метка (баг #2 из отчёта). */
        TextLabel *t = new TextLabel(p, false);
        if (!t->configureInteractive())
        {
            delete t;
            return;
        }
        core_->pushUndoState();
        map->addTextLabel(t);
    }
    else
    {
        return;
    }
    rebuildFromModel();
    emit modelChanged();
}

void TopologyScene::beginLinkCreation(NodeItem *source)
{
    if (!source) return;
    cancelLinkCreation();
    rubberSource_ = source;
    rubberLine_ = new QGraphicsLineItem();
    QPen pen(QColor("#2D6CDF"), 2, Qt::DashLine);
    rubberLine_->setPen(pen);
    rubberLine_->setZValue(50);
    rubberLine_->setLine(QLineF(source->pos(), source->pos()));
    addItem(rubberLine_);
    emit messageRequested(tr("Создание канала: щёлкните по второму узлу. ESC — отмена."));
}

void TopologyScene::cancelLinkCreation()
{
    rubberSource_ = nullptr;
    if (rubberLine_)
    {
        removeItem(rubberLine_);
        delete rubberLine_;
        rubberLine_ = nullptr;
    }
}

void TopologyScene::tryFinishLink(NodeItem *target)
{
    if (!rubberSource_ || !target || target == rubberSource_)
    {
        cancelLinkCreation();
        return;
    }
    if (!core_ || !core_->getMap())
    {
        cancelLinkCreation();
        return;
    }
    NodeItem *src = rubberSource_;
    cancelLinkCreation();

    NetworkMap *map = core_->getMap();
    Node *na = src->domainNode();
    Node *nb = target->domainNode();
    if (!na || !nb) return;

    /* ВАЖНО: имена берём из доменных узлов СЕЙЧАС. rebuildFromModel() ниже
     * удаляет все NodeItem (включая src/target), поэтому обращаться к
     * src->displayName()/target->displayName() ПОСЛЕ rebuild нельзя —
     * это use-after-free (программа падала при создании канала). */
    const QString aName = na->getName();
    const QString bName = nb->getName();

    DeviceType dta = na->getDeviceType();
    DeviceType dtb = nb->getDeviceType();

    /* CSLink: контроллер ↔ свитч (в любом порядке). */
    bool aIsC = (dta == SDNCONTROLLER);
    bool bIsC = (dtb == SDNCONTROLLER);
    bool aIsS = (dta == SWITCH);
    bool bIsS = (dtb == SWITCH);

    if (aIsC && bIsS)
    {
        core_->pushUndoState();
        CSLink *cs = new CSLink(na, nb);
        map->addCSLink(cs);
        rebuildFromModel();
        emit modelChanged();
        emit messageRequested(tr("Связь %1 → %2 создана").arg(aName, bName));
        return;
    }
    if (bIsC && aIsS)
    {
        core_->pushUndoState();
        CSLink *cs = new CSLink(nb, na);
        map->addCSLink(cs);
        rebuildFromModel();
        emit modelChanged();
        emit messageRequested(tr("Связь %1 → %2 создана").arg(aName, bName));
        return;
    }
    if (aIsC || bIsC)
    {
        emit messageRequested(tr("Контроллер можно соединить только с коммутатором."));
        return;
    }

    /* SSLink: всё остальное (свитч-свитч, хост-свитч). Хост-хост не разрешаем. */
    if (dta == HOST && dtb == HOST)
    {
        emit messageRequested(tr("Прямая связь между хостами не поддерживается."));
        return;
    }

    core_->pushUndoState();
    SSLink *ln = new SSLink(na, nb);
    ln->setDelay(10);
    ln->setBandwidth(100);
    ln->setPacketLossRate(0);
    map->addSSLink(ln);
    rebuildFromModel();
    emit modelChanged();
    emit messageRequested(tr("Канал %1—%2 создан").arg(aName, bName));
}

bool TopologyScene::tryHandleDeleteToolClick(const QPointF &scenePos)
{
    if (!core_) return false;
    QList<QGraphicsItem *> hits = this->items(scenePos);
    for (QGraphicsItem *it : hits)
    {
        if (NodeItem *ni = dynamic_cast<NodeItem *>(it))
        {
            if (ni->domainNode())
            {
                core_->pushUndoState();
                core_->getMap()->removeElement(ni->domainNode());
                rebuildFromModel();
                emit modelChanged();
                emit messageRequested(tr("Узел удалён"));
                return true;
            }
        }
        if (LinkItem *li = dynamic_cast<LinkItem *>(it))
        {
            if (li->domainLink())
            {
                core_->pushUndoState();
                core_->getMap()->removeElement(li->domainLink());
                rebuildFromModel();
                emit modelChanged();
                emit messageRequested(tr("Канал удалён"));
                return true;
            }
        }
    }
    return false;
}

void TopologyScene::deleteSelected()
{
    if (!core_) return;
    QList<QGraphicsItem *> sel = selectedItems();
    if (sel.isEmpty())
    {
        emit messageRequested(tr("Ничего не выделено для удаления."));
        return;
    }
    NetworkMap *map = core_->getMap();
    int countN = 0, countL = 0;
    /* Сначала собираем модели — после rebuildFromModel() графические item'ы
     * будут удалены. */
    QList<Node *> nodesToDel;
    QList<Link *> linksToDel;
    for (QGraphicsItem *it : sel)
    {
        if (NodeItem *ni = dynamic_cast<NodeItem *>(it))
        {
            if (ni->domainNode()) nodesToDel.append(ni->domainNode());
        }
        else if (LinkItem *li = dynamic_cast<LinkItem *>(it))
        {
            if (li->domainLink()) linksToDel.append(li->domainLink());
        }
    }
    core_->pushUndoState();
    /* Удаляем линки первыми, потом узлы (на случай если они связаны и
     * disconnectNode удалит линк раньше). */
    for (Link *l : linksToDel)
    {
        map->removeElement(l);
        ++countL;
    }
    for (Node *n : nodesToDel)
    {
        map->removeElement(n);
        ++countN;
    }
    rebuildFromModel();
    emit modelChanged();
    emit messageRequested(tr("Удалено: узлов %1, каналов %2").arg(countN).arg(countL));
}

void TopologyScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (rubberSource_ && rubberLine_)
    {
        rubberLine_->setLine(QLineF(rubberSource_->pos(), event->scenePos()));
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void TopologyScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    /* Идёт rubber-band создание линка — обрабатываем приоритетно. */
    if (rubberSource_)
    {
        if (event->button() == Qt::LeftButton)
        {
            NodeItem *target = nullptr;
            for (QGraphicsItem *it : this->items(event->scenePos()))
            {
                NodeItem *ni = dynamic_cast<NodeItem *>(it);
                if (ni && ni != rubberSource_) { target = ni; break; }
            }
            if (target)
            {
                tryFinishLink(target);
            }
            else
            {
                cancelLinkCreation();
                emit messageRequested(tr("Создание канала отменено."));
            }
            event->accept();
            return;
        }
        if (event->button() == Qt::RightButton)
        {
            cancelLinkCreation();
            event->accept();
            return;
        }
    }

    /* Правый клик: пусть item-ы сами обработают через contextMenuEvent. */
    if (event->button() == Qt::RightButton)
    {
        QList<QGraphicsItem *> hits = this->items(event->scenePos());
        bool onItem = false;
        for (QGraphicsItem *it : hits)
        {
            if (dynamic_cast<NodeItem *>(it) || dynamic_cast<LinkItem *>(it))
            {
                onItem = true;
                break;
            }
        }
        if (!onItem)
        {
            emit emptyContextMenu(event->screenPos());
            event->accept();
            return;
        }
        /* Иначе — пусть item-ы получат свой contextMenuEvent. */
    }

    if (event->button() == Qt::LeftButton)
    {
        /* Инструмент Delete: клик удаляет первый item под курсором. */
        if (activeTool_ == "delete")
        {
            if (tryHandleDeleteToolClick(event->scenePos()))
            {
                event->accept();
                return;
            }
        }
        /* Инструмент создания узла: клик по пустому месту создаёт узел. */
        else if (activeTool_ == "host" || activeTool_ == "switch" ||
                 activeTool_ == "controller" || activeTool_ == "docker" ||
                 activeTool_ == "text")
        {
            QList<QGraphicsItem *> hits = this->items(event->scenePos());
            bool onItem = false;
            for (QGraphicsItem *it : hits)
            {
                if (dynamic_cast<NodeItem *>(it)) { onItem = true; break; }
            }
            if (!onItem)
            {
                createNodeAt(activeTool_, event->scenePos());
                event->accept();
                return;
            }
        }
        /* Инструмент Link: клик по узлу — старт rubber-band. */
        else if (activeTool_ == "link")
        {
            QList<QGraphicsItem *> hits = this->items(event->scenePos());
            NodeItem *src = nullptr;
            for (QGraphicsItem *it : hits)
            {
                src = dynamic_cast<NodeItem *>(it);
                if (src) break;
            }
            if (src)
            {
                beginLinkCreation(src);
                event->accept();
                return;
            }
        }
        /* Select-режим: возможно начинается перетаскивание узла. Снимаем
         * состояние до перемещения — зафиксируем в undo на release, если
         * узел реально сдвинется. */
        else if (core_)
        {
            for (QGraphicsItem *it : this->items(event->scenePos()))
            {
                if (dynamic_cast<NodeItem *>(it))
                {
                    pendingMove_ = true;
                    pendingMoveSnapshot_ = core_->getMap()->createXmlDocument();
                    break;
                }
            }
        }
    }

    QGraphicsScene::mousePressEvent(event);
}

void TopologyScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    if (pendingMove_ && core_)
    {
        pendingMove_ = false;
        /* Если позиция узла(ов) изменилась — XML отличается → фиксируем undo. */
        QString now = core_->getMap()->createXmlDocument();
        if (now != pendingMoveSnapshot_)
        {
            core_->pushUndoSnapshot(pendingMoveSnapshot_);
            emit modelChanged();
        }
        pendingMoveSnapshot_.clear();
    }
}

void TopologyScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        cancelLinkCreation();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
    {
        deleteSelected();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void TopologyScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);
    /* Сетка точек 40px — лёгкий ориентир для пользователя. Цвет
     * адаптируется под тему. */
    painter->save();
    QColor dotColor = (ThemeManager::instance()->currentTheme() == ThemeManager::Dark)
                      ? QColor("#1E293B") : QColor("#E5E7EB");
    QPen dot(dotColor, 1);
    painter->setPen(dot);
    const int step = 40;
    int left = int(rect.left()) - (int(rect.left()) % step);
    int top = int(rect.top()) - (int(rect.top()) % step);
    for (int x = left; x < rect.right(); x += step)
    {
        for (int y = top; y < rect.bottom(); y += step)
        {
            painter->drawPoint(x, y);
        }
    }
    painter->restore();
}

void TopologyScene::onNodeDoubleClicked(NodeItem *n)
{
    emit nodeDoubleClicked(n);
    if (n && n->domainNode()) n->domainNode()->configure();
    rebuildFromModel();
}

void TopologyScene::onNodeContextMenu(NodeItem *n, QPoint globalPos)
{
    emit nodeContextMenu(n, globalPos);
}

void TopologyScene::onLinkSourceRequested(NodeItem *src)
{
    beginLinkCreation(src);
}

void TopologyScene::onLinkDoubleClicked(LinkItem *li)
{
    if (!li || !li->domainLink()) return;
    /* CSLink не имеет диалога настроек — настраиваем только SSLink. */
    SSLink *sl = dynamic_cast<SSLink *>(li->domainLink());
    if (sl)
    {
        sl->configure();
        refreshAllLinkLabels();
        li->update();
        /* После изменения свойств канала — пересчитать маршруты, если
         * выбранный алгоритм требует полного пересчёта. */
        if (core_) core_->recomputeActiveRouting();
    }
    emit linkDoubleClicked(li);
}

void TopologyScene::onLinkContextMenu(LinkItem *li, QPoint globalPos)
{
    emit linkContextMenu(li, globalPos);
}

/* ===== Подсветка маршрутов алгоритмами ===== */
void TopologyScene::clearAllHighlights()
{
    for (LinkItem *li : linkIndex_.values())
    {
        if (li && li->kind() == LinkItem::KindSSLink)
        {
            li->clearHighlight();
        }
    }
    /* Снимаем также подсветку узлов (для сегментов). */
    for (NodeItem *ni : nodeIndex_.values())
    {
        if (ni) ni->clearHighlight();
    }
    if (animator_) animator_->clearAll();
    lastHlMode_ = HL_None;
    lastHlTree_.clear();
    lastHlPath_.clear();
    lastHlSegments_.clear();
    lastHlTrees_.clear();
    lastHlTreeColors_.clear();
}

void TopologyScene::highlightTreesByGroupId(const QList<QList<QList<int>>> &trees,
                                            const QList<QColor> &colors)
{
    if (!core_) return;
    NetworkMap *map = core_->getMap();
    if (!map) return;
    clearAllHighlights();
    if (trees.isEmpty()) return;

    lastHlMode_ = HL_MultiTree;
    lastHlTrees_ = trees;
    lastHlTreeColors_ = colors;

    QHash<int, Switch *> idxOfSw;
    for (Switch *sw : map->getSwitches()) idxOfSw[sw->getGroupId()] = sw;

    /* Рисуем деревья по порядку — последнее «побеждает» на общих рёбрах. */
    for (int t = 0; t < trees.size(); ++t)
    {
        QColor color = (t < colors.size()) ? colors[t] : QColor("#DC2626");
        for (const QList<int> &edge : trees[t])
        {
            if (edge.size() != 2) continue;
            Switch *a = idxOfSw.value(edge[0], nullptr);
            Switch *b = idxOfSw.value(edge[1], nullptr);
            if (!a || !b) continue;
            for (SSLink *l : map->getSSLinks())
            {
                if ((l->getNode1() == a && l->getNode2() == b) ||
                    (l->getNode1() == b && l->getNode2() == a))
                {
                    LinkItem *li = findItem(static_cast<Link *>(l));
                    if (li) li->setHighlightColor(color);
                    break;
                }
            }
        }
    }
}

void TopologyScene::highlightSegmentsByGroupId(const QList<QList<int>> &segments)
{
    if (!core_) return;
    NetworkMap *map = core_->getMap();
    if (!map) return;

    /* Сначала снимаем все подсветки. */
    clearAllHighlights();
    /* Запоминаем для переприменения после rebuildFromModel. */
    lastHlMode_ = HL_Segments;
    lastHlSegments_ = segments;

    /* Палитра 8 цветов (Indigo-Blue compatible). */
    static const QColor palette[] = {
        QColor("#2D6CDF"),  // синий
        QColor("#22C55E"),  // зелёный
        QColor("#F59E0B"),  // оранжевый
        QColor("#A78BFA"),  // фиолетовый
        QColor("#EC4899"),  // розовый
        QColor("#0EA5E9"),  // голубой
        QColor("#14B8A6"),  // teal
        QColor("#EAB308"),  // жёлтый
    };
    const int paletteN = sizeof(palette) / sizeof(palette[0]);

    QHash<int, Switch *> idxOfSw;
    for (Switch *sw : map->getSwitches()) idxOfSw[sw->getGroupId()] = sw;

    /* Узлы — подсветка обводкой того же цвета сегмента. */
    QHash<Switch *, QColor> swColor;
    for (int s = 0; s < segments.size(); ++s)
    {
        QColor color = palette[s % paletteN];
        for (int gid : segments[s])
        {
            Switch *sw = idxOfSw.value(gid, nullptr);
            if (!sw) continue;
            swColor[sw] = color;
            NodeItem *ni = findItem(static_cast<Node *>(sw));
            if (ni) ni->setHighlightColor(color);
        }
    }

    /* Внутрисегментные рёбра — окрашиваем цветом сегмента (сплошные).
     * Межсегментные рёбра (концы в разных сегментах) — отдельный стиль:
     * толстая пунктирная линия пурпурного цвета, чтобы было сразу
     * видно «мост» между двумя сегментами. */
    static const QColor kInterSegColor("#DC2626");   // алый для контраста
    for (SSLink *link : map->getSSLinks())
    {
        Switch *s1 = dynamic_cast<Switch *>(link->getNode1());
        Switch *s2 = dynamic_cast<Switch *>(link->getNode2());
        if (!s1 || !s2) continue;
        auto it1 = swColor.constFind(s1);
        auto it2 = swColor.constFind(s2);
        if (it1 == swColor.constEnd() || it2 == swColor.constEnd()) continue;
        LinkItem *li = findItem(static_cast<Link *>(link));
        if (!li) continue;
        if (it1.value() == it2.value())
        {
            li->setHighlightColor(it1.value());
            li->setInterSegment(false);
        }
        else
        {
            li->setHighlightColor(kInterSegColor);
            li->setInterSegment(true);
        }
    }
}

void TopologyScene::highlightTreeByGroupId(const QList<QList<int>> &tree, QColor color)
{
    if (!core_) return;
    NetworkMap *map = core_->getMap();
    if (!map) return;
    /* Сначала сбрасываем все подсветки SSLink. */
    clearAllHighlights();
    lastHlMode_ = HL_Tree;
    lastHlTree_ = tree;
    lastHlColor_ = color;

    /* Строим индекс: groupId -> Switch*. */
    QHash<int, Switch *> idxOfSw;
    for (Switch *sw : map->getSwitches())
    {
        idxOfSw[sw->getGroupId()] = sw;
    }

    for (const QList<int> &edge : tree)
    {
        if (edge.size() != 2) continue;
        Switch *a = idxOfSw.value(edge[0], nullptr);
        Switch *b = idxOfSw.value(edge[1], nullptr);
        if (!a || !b) continue;
        /* Ищем SSLink, соединяющий эти два свитча. */
        for (SSLink *l : map->getSSLinks())
        {
            if ((l->getNode1() == a && l->getNode2() == b) ||
                (l->getNode1() == b && l->getNode2() == a))
            {
                LinkItem *li = findItem(static_cast<Link *>(l));
                if (li) li->setHighlightColor(color);
                break;
            }
        }
    }
}

void TopologyScene::highlightPathByGroupId(const QList<int> &path, QColor color)
{
    QList<QList<int>> edges;
    for (int i = 0; i + 1 < path.size(); ++i)
    {
        edges.append({path[i], path[i + 1]});
    }
    highlightTreeByGroupId(edges, color);
    /* highlightTreeByGroupId выставит lastHlMode_=HL_Tree, поправляем
     * на HL_Path и кладём оригинальный path — пригодится при rebuild. */
    lastHlMode_ = HL_Path;
    lastHlPath_ = path;
    lastHlTree_.clear();
}

void TopologyScene::animatePacketBetweenHosts(const QString &hostFrom,
                                              const QString &hostTo, QColor color)
{
    if (!core_ || !animator_) return;
    NetworkMap *map = core_->getMap();
    if (!map) return;

    Node *src = map->getNodeByName(hostFrom);
    Node *dst = map->getNodeByName(hostTo);
    if (!src || !dst) return;

    /* 1. Сначала пробуем построить маршрут через выбранный алгоритм
     * (Dijkstra / LARAC / Greedy). */
    QList<int> routeGroupIds = core_->computeRoutePath(hostFrom, hostTo);
    if (!routeGroupIds.isEmpty())
    {
        QList<LinkItem *> chain;
        QHash<int, Switch *> idxOfSw;
        for (Switch *sw : map->getSwitches())
        {
            idxOfSw[sw->getGroupId()] = sw;
        }
        Node *cur = src;
        /* host → первый свитч маршрута */
        Switch *firstSw = idxOfSw.value(routeGroupIds.first(), nullptr);
        if (firstSw && cur != firstSw)
        {
            for (SSLink *l : map->getSSLinks())
            {
                if ((l->getNode1() == cur && l->getNode2() == firstSw) ||
                    (l->getNode2() == cur && l->getNode1() == firstSw))
                {
                    LinkItem *li = findItem(static_cast<Link *>(l));
                    if (li) chain.append(li);
                    cur = firstSw;
                    break;
                }
            }
        }
        else if (firstSw)
        {
            cur = firstSw;
        }
        /* серия свитчей */
        for (int i = 0; i + 1 < routeGroupIds.size(); ++i)
        {
            Switch *a = idxOfSw.value(routeGroupIds[i], nullptr);
            Switch *b = idxOfSw.value(routeGroupIds[i + 1], nullptr);
            if (!a || !b) { chain.clear(); break; }
            bool found = false;
            for (SSLink *l : map->getSSLinks())
            {
                if ((l->getNode1() == a && l->getNode2() == b) ||
                    (l->getNode1() == b && l->getNode2() == a))
                {
                    LinkItem *li = findItem(static_cast<Link *>(l));
                    if (li) { chain.append(li); found = true; }
                    break;
                }
            }
            if (!found) { chain.clear(); break; }
            cur = b;
        }
        /* последний свитч → host-получатель */
        Switch *lastSw = idxOfSw.value(routeGroupIds.last(), nullptr);
        if (lastSw && dst != lastSw)
        {
            for (SSLink *l : map->getSSLinks())
            {
                if ((l->getNode1() == lastSw && l->getNode2() == dst) ||
                    (l->getNode2() == lastSw && l->getNode1() == dst))
                {
                    LinkItem *li = findItem(static_cast<Link *>(l));
                    if (li) chain.append(li);
                    break;
                }
            }
        }
        if (!chain.isEmpty())
        {
            animator_->playOnce(chain, color, 1400);
            return;
        }
    }

    /* 2. Fallback — BFS по графу (когда алгоритм не справился). */

    /* Простейший BFS по всем линкам сцены (SSLink). Используем подсвеченные
     * (highlight) линки если они есть — это путь, найденный алгоритмом.
     * Иначе — кратчайший в количестве хопов. */
    QHash<Node *, QList<QPair<Node *, LinkItem *>>> adj;
    for (LinkItem *li : linkIndex_.values())
    {
        if (!li || li->kind() != LinkItem::KindSSLink) continue;
        Link *l = li->domainLink();
        if (!l) continue;
        Node *a = l->getNode1();
        Node *b = l->getNode2();
        if (!a || !b) continue;
        adj[a].append(qMakePair(b, li));
        adj[b].append(qMakePair(a, li));
    }

    /* BFS — кратчайший по числу хопов путь src→dst. */
    QHash<Node *, Node *> prev;
    QHash<Node *, LinkItem *> prevLink;
    QList<Node *> queue;
    queue.append(src);
    prev[src] = src;
    while (!queue.isEmpty())
    {
        Node *cur = queue.takeFirst();
        if (cur == dst) break;
        for (const auto &pair : adj.value(cur))
        {
            Node *nxt = pair.first;
            LinkItem *li = pair.second;
            if (prev.contains(nxt)) continue;
            prev[nxt] = cur;
            prevLink[nxt] = li;
            queue.append(nxt);
        }
    }
    if (!prev.contains(dst)) return;

    /* Восстанавливаем последовательность LinkItem от src к dst. */
    QList<LinkItem *> chain;
    Node *cur = dst;
    while (cur != src)
    {
        LinkItem *li = prevLink.value(cur, nullptr);
        if (!li) return;
        chain.prepend(li);
        cur = prev.value(cur, nullptr);
        if (!cur) return;
    }
    animator_->playOnce(chain, color, 1400);
}
