#ifndef TOPOLOGYVIEW_H
#define TOPOLOGYVIEW_H

#include <QGraphicsView>
#include <QPoint>
#include <QString>

class TopologyScene;

/* TopologyView — холст-навигатор.
 *
 * Поведение мыши/тачпада:
 *   • Колесо без модификатора             — zoom in/out (как в Figma)
 *   • ЛКМ по пустому месту + drag         — RubberBand selection (как в Figma)
 *   • Средняя кнопка мыши + drag          — pan
 *   • Пробел + ЛКМ + drag                 — pan (альтернатива)
 *   • ЛКМ по узлу + drag                  — перемещение узла (сама scene)
 *   • Shift+ЛКМ по узлу                   — начать создание линка (резиновая линия)
 *   • Drag-n-drop из палитры              — создать узел в точке drop'а
 *
 * Скроллбары скрыты, прокрутка только через pan.
 */
class TopologyView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit TopologyView(QWidget *parent = nullptr);
    void setTopologyScene(TopologyScene *scene);

    double zoom() const { return currentZoom_; }
    void setZoom(double factor, QPointF anchorScenePos = QPointF());
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToContent();

signals:
    void zoomChanged(double newZoom);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    TopologyScene *topoScene_;
    double currentZoom_;
    bool panning_;
    bool spaceHeld_;
    QPoint lastPanPoint_;
};

#endif // TOPOLOGYVIEW_H
