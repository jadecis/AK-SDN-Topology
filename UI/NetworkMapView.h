#ifndef NETWORKMAPVIEW_H
#define NETWORKMAPVIEW_H

#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>

/* NetworkMapView — холст для отрисовки сетевой топологии.
 * Расширения по сравнению с базовым QLabel:
 *   • zoom: setZoom/zoomIn/zoomOut, текущий коэффициент в zoom().
 *   • Ctrl+колесо мыши изменяет масштаб (10% шаг).
 *   • mapToCanvas() возвращает координаты исходного pixmap'а (без zoom),
 *     чтобы Core продолжал работать в исходной системе координат.
 *   • Сохраняет оригинальный pixmap (originalPixmap_) для пересборки
 *     при изменении zoom.
 *   • Сигналы для мыши и контекстного меню.
 */
class NetworkMapView : public QLabel
{
    Q_OBJECT

public:
    explicit NetworkMapView(QWidget *parent = 0);

    void refresh(QPixmap pix);

    double zoom() const { return zoom_; }
    void setZoom(double factor);
    void zoomIn();
    void zoomOut();

    QSize originalPixmapSize() const { return originalPixmap_.size(); }

    /* Преобразует координату клика view → координату оригинального pixmap'а. */
    QPoint mapToCanvas(QPoint viewPos) const;

private:
    void initialize();
    void applyZoom();

protected:
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);

signals:
    void signalMouseLeftButtonReleased(QPoint cursorPosition);
    void signalMouseLeftButtonPressed(QPoint cursorPosition);
    void signalMouseMoved(QPoint cursorPosition);
    void signalMouseDoubleClicked(QPoint cursorPosition);
    /* Правый клик: localPos — позиция в координатах канвы (для поиска узла),
     * globalPos — для открытия QMenu в нужном месте экрана. */
    void signalRightClickRequested(QPoint localPos, QPoint globalPos);
    void signalZoomChanged(double newZoom);

private:
    QPixmap originalPixmap_;
    double zoom_;
};

#endif // NETWORKMAPVIEW_H
