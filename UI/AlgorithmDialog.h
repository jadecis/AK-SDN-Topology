#ifndef ALGORITHMDIALOG_H
#define ALGORITHMDIALOG_H

#include <QDialog>
#include <QString>
#include <QVariantMap>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QWidget;
class QFormLayout;
class QPushButton;
class QNetworkAccessManager;
class NetworkMap;

/* AlgorithmDialog — окно запуска встроенных алгоритмов маршрутизации.
 * Позволяет выбрать:
 *   • алгоритм (из AlgorithmRegistry);
 *   • корневой коммутатор (sN);
 *   • метрику веса канала (delay / bandwidth / loss);
 *   • активную маршрутизацию (галочка) — на стороне Ryu будут установлены
 *     OpenFlow flow-правила, и реальные пакеты пойдут по найденным маршрутам.
 *
 * При нажатии «Запустить» испускает signalRun(algorithmId, rootIndex, metric)
 * и (если включена активная маршрутизация) дополнительно отправляет
 * HTTP-запрос на Ryu /route/activate. */
class AlgorithmDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AlgorithmDialog(NetworkMap *map, QWidget *parent = nullptr);

signals:
    void signalRun(QString algorithmId, int rootSwitchIndex, QString metric);
    /* Расширенный сигнал — несёт дополнительные параметры алгоритма
     * (число сегментов, ограничения и т.п.). Core может использовать их
     * вместо хардкодных дефолтов. */
    void signalRunWithParams(QString algorithmId, int rootSwitchIndex,
                              QString metric, QVariantMap params);
    /* Просим главное окно показать статусное сообщение. */
    void signalStatus(QString text);

private slots:
    void onRunClicked();
    void onClearFlowsClicked();
    void onAlgorithmChanged();

private:
    QComboBox *cbAlgorithm;
    QComboBox *cbRoot;
    QComboBox *cbMetric;
    QCheckBox *chkActive;
    QPushButton *btnClearFlows;
    QLabel *infoLabel;
    QLabel *statusLabel;
    NetworkMap *map_;
    QNetworkAccessManager *nam_;

    /* Динамические поля параметров — показываются по выбранному алгоритму. */
    QWidget *rootRow_;          // строка «Корневой коммутатор»
    QSpinBox *spinSegments_;    // K для сегментирования
    QWidget *segRow_;           // строка «Количество сегментов»
    QComboBox *cbMetric2_;      // вторая метрика (LARAC/MCP/CSP)
    QWidget *metric2Row_;
    QDoubleSpinBox *spinRestriction1_;
    QWidget *restr1Row_;
    QDoubleSpinBox *spinRestriction2_;
    QWidget *restr2Row_;
    QDoubleSpinBox *spinLambda_;
    QWidget *lambdaRow_;

    void callRyu(const QString &endpoint);
    void setupDynamicRows(QFormLayout *form, QWidget *parent);
};

#endif // ALGORITHMDIALOG_H
