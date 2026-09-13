#ifndef PALETTEWIDGET_H
#define PALETTEWIDGET_H

#include <QWidget>
#include <QList>
#include <QString>

class QLabel;
class QToolButton;
class QButtonGroup;

/* PaletteWidget — единая палитра инструментов слева. Объединяет:
 *   1. Инструменты редактирования (выделение / создание узлов / линк / удаление)
 *   2. Drag-source-плитки (можно дополнительно перетащить из палитры
 *      на нужную точку холста — удобнее, чем кликать инструмент)
 *
 * Активный инструмент управляется QButtonGroup (exclusive). При смене
 * сигналом toolSelected() уведомляется TopologyScene::setActiveTool.
 *
 * Дополнительно поддерживается drag-n-drop: те же кнопки служат drag-source'ами
 * с MIME "application/x-snet-tool"="host"|"switch"|"controller".
 */
class PaletteWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PaletteWidget(QWidget *parent = nullptr);

    /* Программно установить активный инструмент (например, после ESC). */
    void setActiveTool(const QString &id);
    QString activeTool() const { return activeTool_; }

signals:
    void toolSelected(QString id);

private slots:
    void onButtonClicked();

private:
    QString activeTool_;
    QButtonGroup *group_;
    QList<QToolButton *> buttons_;

    QToolButton *addToolButton(const QString &id, const QString &iconRes,
                               const QString &label, const QString &tip,
                               bool draggable, const QString &shortcut = QString());
    void buildButtons();
};

#endif // PALETTEWIDGET_H
