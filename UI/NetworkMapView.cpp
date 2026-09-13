#include "NetworkMapView.h"
#include <QWheelEvent>

NetworkMapView::NetworkMapView(QWidget *parent) :
    QLabel(parent),
    zoom_(1.0)
{
    initialize();
}

void NetworkMapView::initialize()
{
    setText("");
    setMinimumSize(QSize(1200, 800));
    setAlignment(Qt::AlignTop | Qt::AlignLeft);
}

void NetworkMapView::refresh(QPixmap pix)
{
    originalPixmap_ = pix;
    applyZoom();
}

void NetworkMapView::applyZoom()
{
    if (originalPixmap_.isNull()) return;
    QSize scaled = originalPixmap_.size() * zoom_;
    QPixmap pix = originalPixmap_.scaled(scaled, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
    setPixmap(pix);
    /* Обновляем minimum size, чтобы QScrollArea корректно показала скроллбары. */
    setMinimumSize(pix.size());
    resize(pix.size());
}

void NetworkMapView::setZoom(double factor)
{
    factor = qBound(0.1, factor, 4.0);
    if (qFuzzyCompare(factor, zoom_)) return;
    zoom_ = factor;
    applyZoom();
    emit signalZoomChanged(zoom_);
}

void NetworkMapView::zoomIn()  { setZoom(zoom_ * 1.1); }
void NetworkMapView::zoomOut() { setZoom(zoom_ / 1.1); }

QPoint NetworkMapView::mapToCanvas(QPoint viewPos) const
{
    if (zoom_ <= 0.0) return viewPos;
    return QPoint(int(viewPos.x() / zoom_), int(viewPos.y() / zoom_));
}

void NetworkMapView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else
            zoomOut();
        event->accept();
        return;
    }
    QLabel::wheelEvent(event);
}

void NetworkMapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() & Qt::LeftButton)
    {
        emit signalMouseLeftButtonReleased(event->pos());
    }
}

void NetworkMapView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() & Qt::LeftButton)
    {
        emit signalMouseLeftButtonPressed(event->pos());
    }
    else if (event->button() & Qt::RightButton)
    {
        emit signalRightClickRequested(event->pos(), event->globalPos());
    }
}

void NetworkMapView::mouseMoveEvent(QMouseEvent *event)
{
    emit signalMouseMoved(event->pos());
}

void NetworkMapView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() & Qt::LeftButton)
    {
        emit signalMouseDoubleClicked(event->pos());
    }
}
