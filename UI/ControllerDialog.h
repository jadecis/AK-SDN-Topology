#ifndef CONTROLLERDIALOG_H
#define CONTROLLERDIALOG_H

#include <QDialog>

class SdnController;
class QLineEdit;
class QSpinBox;
class QComboBox;

/* ControllerDialog — современный диалог свойств SDN-контроллера.
 * Поля:
 *   • Имя (c0, c1, …)
 *   • IP-адрес (валидация IPv4)
 *   • Порт (1..65535, по умолчанию 6653)
 *   • Тип (Remote Ryu / Local Reference)
 */
class ControllerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ControllerDialog(SdnController *controller, QWidget *parent = nullptr);

private slots:
    void onAccept();

private:
    SdnController *controller_;
    QLineEdit *edName_;
    QLineEdit *edIp_;
    QSpinBox *spPort_;
    QComboBox *cbType_;

    void buildUi();
};

#endif // CONTROLLERDIALOG_H
