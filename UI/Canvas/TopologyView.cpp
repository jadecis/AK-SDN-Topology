#include "TopologyView.h"
#include "TopologyScene.h"
#include "NodeItem.h"
#include "ThemeManager.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScrollBar>
#include <QApplication>

TopologyView::TopologyView(QWidget *parent) :
    QGraphicsView(parent),
    topoScene_(nullptr),
    currentZoom_(1.0),
    panning_(false),
    spaceHeld_(false)
{
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    /* RubberBandDrag: ЛКМ по пустому месту → выделение области. */
    setDragMode(QGraphicsView::RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);

    /* Без скроллбаров — навигация через pan/zoom. */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setAcceptDrops(true);
    setMouseTracking(true);
    /* Фон View тоже зависит от темы — иначе при тёмной теме видна
     * белая «дыра» по краям сцены при панорамировании. */
    auto applyBg = [this]() {
        QColor bg = (ThemeManager::instance()->currentTheme() == ThemeManager::Dark)
                    ? QColor("#0B1424") : QColor("#FFFFFF");
        setBackgroundBrush(bg);
    };
    applyBg();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [applyBg](ThemeManager::Theme) { applyBg(); });
    setFrameShape(QFrame::NoFrame);

    /* Чтобы клавиатура (Space, Delete) приходила во view. */
    setFocusPolicy(Qt::StrongFocus);

    setCursor(Qt::ArrowCursor);
}

void TopologyView::setTopologyScene(TopologyScene *scene)
{
    topoScene_ = scene;
    setScene(scene);
}

void TopologyView::setZoom(double factor, QPointF anchorScenePos)
{
    factor = qBound(0.1, factor, 5.0);
    double scale = factor / currentZoom_;
    currentZoom_ = factor;
    QTransform t = transform();
    t.scale(scale, scale);
    setTransform(t);
    if (!anchorScenePos.isNull())
    {
        centerOn(anchorScenePos);
    }
    emit zoomChanged(currentZoom_);
}

void TopologyView::zoomIn()  { setZoom(currentZoom_ * 1.15); }
void TopologyView::zoomOut() { setZoom(currentZoom_ / 1.15); }
void TopologyView::resetZoom() { setZoom(1.0); }

void TopologyView::fitToContent()
{
    if (!scene()) return;
    QRectF bounds = scene()->itemsBoundingRect();
    if (bounds.isEmpty()) return;
    bounds.adjust(-80, -80, 80, 80);
    fitInView(bounds, Qt::KeepAspectRatio);
    currentZoom_ = transform().m11();
    emit zoomChanged(currentZoom_);
}

void TopologyView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0)
        zoomIn();
    else
        zoomOut();
    event->accept();
}

void TopologyView::mousePressEvent(QMouseEvent *event)
{
    /* Pan: средняя кнопка ИЛИ ЛКМ при удержанном Space. */
    bool wantPan =
        (event->button() == Qt::MiddleButton) ||
        (event->button() == Qt::LeftButton && spaceHeld_);
    if (wantPan)
    {
        panning_ = true;
        lastPanPoint_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void TopologyView::mouseMoveEvent(QMouseEvent *event)
{
    if (panning_)
    {
        QPoint delta = event->pos() - lastPanPoint_;
        lastPanPoint_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void TopologyView::mouseReleaseEvent(QMouseEvent *event)
{
    if (panning_ && (event->button() == Qt::LeftButton ||
                      event->button() == Qt::MiddleButton))
    {
        panning_ = false;
        setCursor(spaceHeld_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void TopologyView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-snet-tool"))
    {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void TopologyView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-snet-tool"))
    {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void TopologyView::dropEvent(QDropEvent *event)
{
    if (!topoScene_) return;
    if (event->mimeData()->hasFormat("application/x-snet-tool"))
    {
        QString kind = QString::fromUtf8(
            event->mimeData()->data("application/x-snet-tool"));
        QPointF scenePos = mapToScene(event->pos());
        topoScene_->createNodeAt(kind, scenePos);
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dropEvent(event);
}

void TopologyView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        spaceHeld_ = true;
        if (!panning_) setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal)
    {
        zoomIn(); event->accept(); return;
    }
    if (event->key() == Qt::Key_Minus)
    {
        zoomOut(); event->accept(); return;
    }
    if (event->key() == Qt::Key_0 && (event->modifiers() & Qt::ControlModifier))
    {
        resetZoom(); event->accept(); return;
    }
    QGraphicsView::keyPressEvent(event);
}

void TopologyView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
    {
        spaceHeld_ = false;
        if (!panning_) setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}
