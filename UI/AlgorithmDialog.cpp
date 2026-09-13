#include "AlgorithmDialog.h"
#include "AlgorithmRegistry.h"
#include "IRoutingAlgorithm.h"
#include "NetworkMap.h"
#include "Switch.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

AlgorithmDialog::AlgorithmDialog(NetworkMap *map, QWidget *parent) :
    QDialog(parent),
    map_(map),
    nam_(new QNetworkAccessManager(this)),
    rootRow_(nullptr),
    spinSegments_(nullptr),
    segRow_(nullptr),
    cbMetric2_(nullptr),
    metric2Row_(nullptr),
    spinRestriction1_(nullptr),
    restr1Row_(nullptr),
    spinRestriction2_(nullptr),
    restr2Row_(nullptr),
    spinLambda_(nullptr),
    lambdaRow_(nullptr)
{
    setWindowTitle(tr("Алгоритмы маршрутизации"));
    setWindowIcon(ThemeManager::instance()->makeIcon(":/modern/modern/algorithm.svg",
                                                     QSize(48, 48)));
    setMinimumWidth(720);
    setSizeGripEnabled(true);

    QVBoxLayout *outer = new QVBoxLayout(this);

    QGroupBox *paramsBox = new QGroupBox(tr("Параметры"), this);
    QFormLayout *form = new QFormLayout(paramsBox);
    form->setLabelAlignment(Qt::AlignRight);
    /* AllNonFixedFieldsGrow + Wrap=Dont — гарантирует, что spinbox'ы
     * не используют свой sizeHint, а растягиваются на всю ширину как
     * комбобоксы выше. Без этого LARAC-поля R₁/R₂/λ оставались узкими,
     * текст "30.00" наезжал на стрелки. */
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(16);
    form->setContentsMargins(16, 18, 16, 18);

    const int kFieldH = 34;
    /* Локальный stylesheet делает кнопки-стрелки внутри QDoubleSpinBox
     * шире и явно отделёнными от текста. Без него штатные стрелки 8 px
     * клипают значения «30.00» в LARAC-полях (видно на скриншоте). */
    paramsBox->setStyleSheet(
        "QDoubleSpinBox, QSpinBox {"
        "  padding-right: 24px;"
        "  min-height: 30px;"
        "}"
        "QDoubleSpinBox::up-button, QSpinBox::up-button,"
        "QDoubleSpinBox::down-button, QSpinBox::down-button {"
        "  width: 20px;"
        "}"
    );

    cbAlgorithm = new QComboBox(paramsBox);
    cbAlgorithm->setMinimumHeight(kFieldH);
    cbAlgorithm->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for (IRoutingAlgorithm *a : AlgorithmRegistry::instance()->all())
    {
        cbAlgorithm->addItem(a->displayName(), a->id());
    }
    form->addRow(tr("Алгоритм:"), cbAlgorithm);

    cbRoot = new QComboBox(paramsBox);
    cbRoot->setMinimumHeight(kFieldH);
    cbRoot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (map_)
    {
        QList<Switch *> switches = map_->getSwitches();
        for (int i = 0; i < switches.size(); ++i)
        {
            cbRoot->addItem(switches[i]->getName(), i);
        }
    }
    /* Сохраняем виджет строки, чтобы скрывать его для сегментирования. */
    rootRow_ = cbRoot;
    form->addRow(tr("Корневой коммутатор:"), cbRoot);

    cbMetric = new QComboBox(paramsBox);
    cbMetric->setMinimumHeight(kFieldH);
    cbMetric->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cbMetric->addItem(tr("Задержка (delay)"), "delay");
    cbMetric->addItem(tr("Пропускная способность (bandwidth)"), "bandwidth");
    cbMetric->addItem(tr("Потери пакетов (loss)"), "loss");
    form->addRow(tr("Метрика:"), cbMetric);

    setupDynamicRows(form, paramsBox);

    outer->addWidget(paramsBox);

    /* Активная маршрутизация. */
    QGroupBox *activeBox = new QGroupBox(tr("Активная маршрутизация"), this);
    QVBoxLayout *activeLayout = new QVBoxLayout(activeBox);
    chkActive = new QCheckBox(tr("Установить flow-правила в Ryu (трафик пойдёт "
                                  "по найденным маршрутам)"), activeBox);
    chkActive->setChecked(false);
    QLabel *activeHint = new QLabel(
        tr("<i>При включении SNet после расчёта алгоритма отправит запрос в "
           "Ryu (<code>http://localhost:8080/route/activate</code>), и контроллер "
           "установит OpenFlow-правила на каждый свитч по найденному пути. "
           "Реальные пакеты (ping, curl, psql) пойдут по этим маршрутам.<br>"
           "Чтобы вернуться к обычному L2 learning — кнопка ниже.</i>"),
        activeBox);
    activeHint->setWordWrap(true);
    activeHint->setStyleSheet("color: #94A3B8; padding: 4px;");

    QHBoxLayout *clearRow = new QHBoxLayout();
    btnClearFlows = new QPushButton(tr("Сбросить flow-правила (вернуть L2)"), activeBox);
    btnClearFlows->setAutoDefault(false);
    /* Делаем кнопку «опасной» через свойство, а не через жёсткий styleSheet —
     * иначе на тёмной теме светло-розовая заливка с тёмным текстом
     * выглядит инородно. ThemeManager уважает свойство «danger». */
    btnClearFlows->setProperty("danger", true);
    clearRow->addWidget(btnClearFlows);
    clearRow->addStretch();

    activeLayout->addWidget(chkActive);
    activeLayout->addLayout(clearRow);
    activeLayout->addWidget(activeHint);
    outer->addWidget(activeBox);

    /* Подсказки + статус */
    infoLabel = new QLabel(this);
    infoLabel->setWordWrap(true);
    if (cbRoot->count() == 0)
    {
        infoLabel->setText(tr("⚠ В топологии нет коммутаторов. Добавьте хотя бы один "
                              "коммутатор (s1) и пару каналов связи, чтобы запустить алгоритм."));
        infoLabel->setStyleSheet("color: #B45309;");
    }
    else
    {
        infoLabel->setText(tr("Результат отобразится на карте как дерево оптимальных маршрутов "
                              "(красные рёбра) или сегменты (цветные кольца на узлах)."));
    }
    outer->addWidget(infoLabel);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet("color:#0F766E; padding:4px;");
    outer->addWidget(statusLabel);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *btnCancel = new QPushButton(tr("Отмена"), this);
    btnCancel->setAutoDefault(false);
    btnCancel->setProperty("secondary", true);    // тема сделает её бесцветной
    QPushButton *btnRun = new QPushButton(tr("Запустить"), this);
    btnRun->setDefault(true);
    btnRun->setEnabled(cbRoot->count() > 0);

    buttons->addWidget(btnCancel);
    buttons->addWidget(btnRun);
    outer->addLayout(buttons);

    connect(btnCancel, &QPushButton::clicked, this, &AlgorithmDialog::reject);
    connect(btnRun, &QPushButton::clicked, this, &AlgorithmDialog::onRunClicked);
    connect(btnClearFlows, &QPushButton::clicked,
            this, &AlgorithmDialog::onClearFlowsClicked);
    connect(cbAlgorithm, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AlgorithmDialog::onAlgorithmChanged);

    /* Инициализация видимости полей по дефолтному алгоритму. */
    onAlgorithmChanged();
}

void AlgorithmDialog::setupDynamicRows(QFormLayout *form, QWidget *parent)
{
    /* Количество сегментов K — только для сегментирования. По умолчанию
     * 2 (минимально осмысленно для демонстрации), максимум = количество
     * коммутаторов в топологии. */
    int switchCount = map_ ? map_->getSwitches().size() : 8;
    spinSegments_ = new QSpinBox(parent);
    spinSegments_->setRange(2, qMax(2, switchCount));
    spinSegments_->setValue(2);
    spinSegments_->setSuffix(tr(" сегментов"));
    spinSegments_->setMinimumWidth(280);
    spinSegments_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    segRow_ = spinSegments_;
    form->addRow(tr("Количество сегментов (K):"), spinSegments_);

    /* Вторая метрика — для алгоритмов QoS с двумя метриками (LARAC, MCP, ...). */
    cbMetric2_ = new QComboBox(parent);
    cbMetric2_->setMinimumHeight(34);
    cbMetric2_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cbMetric2_->addItem(tr("Потери пакетов (loss)"), "loss");
    cbMetric2_->addItem(tr("Задержка (delay)"), "delay");
    cbMetric2_->addItem(tr("Пропускная способность (bandwidth)"), "bandwidth");
    metric2Row_ = cbMetric2_;
    form->addRow(tr("Вторая метрика (ограничение):"), cbMetric2_);

    /* Бюджет / ограничение по первой метрике. */
    spinRestriction1_ = new QDoubleSpinBox(parent);
    spinRestriction1_->setRange(0.0, 1e6);
    spinRestriction1_->setValue(30.0);
    spinRestriction1_->setDecimals(2);
    spinRestriction1_->setMinimumWidth(280);
    spinRestriction1_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    restr1Row_ = spinRestriction1_;
    form->addRow(tr("Ограничение R₁ (по 1-й метрике):"), spinRestriction1_);

    /* Бюджет / ограничение по второй метрике (R₂). Это единственное активное
     * ограничение классического LARAC: путь минимизируется по 1-й метрике
     * при суммарной 2-й метрике ≤ R₂. По умолчанию 0 — строжайший режим
     * (например «совсем без потерь»). */
    spinRestriction2_ = new QDoubleSpinBox(parent);
    spinRestriction2_->setRange(0.0, 1e6);
    spinRestriction2_->setValue(0.0);
    spinRestriction2_->setDecimals(2);
    spinRestriction2_->setMinimumWidth(280);
    spinRestriction2_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    restr2Row_ = spinRestriction2_;
    form->addRow(tr("Ограничение R₂ (по 2-й метрике):"), spinRestriction2_);

    /* Лагранжев коэффициент λ. 0 = авто-LARAC (множитель подбирается так,
     * чтобы уложиться в R₂). λ>0 — ручной режим: один проход по
     * cost + λ·constr (демонстрация фиксированного множителя). */
    spinLambda_ = new QDoubleSpinBox(parent);
    spinLambda_->setRange(0.0, 100.0);
    spinLambda_->setValue(0.0);
    spinLambda_->setSingleStep(0.1);
    spinLambda_->setDecimals(2);
    spinLambda_->setMinimumWidth(280);
    spinLambda_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    lambdaRow_ = spinLambda_;
    form->addRow(tr("Коэффициент λ (0 = авто):"), spinLambda_);
}

static void setRowVisible(QFormLayout *form, QWidget *field, bool vis)
{
    if (!form || !field) return;
    int row = -1;
    QFormLayout::ItemRole role;
    form->getWidgetPosition(field, &row, &role);
    if (row < 0) return;
    if (QLayoutItem *li = form->itemAt(row, QFormLayout::LabelRole))
    {
        if (QWidget *lbl = li->widget()) lbl->setVisible(vis);
    }
    if (QLayoutItem *li = form->itemAt(row, QFormLayout::FieldRole))
    {
        if (QWidget *fw = li->widget()) fw->setVisible(vis);
    }
}

void AlgorithmDialog::onAlgorithmChanged()
{
    /* Прячем/показываем поля в зависимости от выбранного алгоритма.
     * Используем displayName(), т.к. совпадает с Ryu-стороной. */
    QString name = cbAlgorithm->currentText();
    bool isSeg = (name == QStringLiteral("Жадное сегментирование"));
    bool isLARAC = (name == QStringLiteral("LARAC"));

    QFormLayout *form = nullptr;
    if (cbRoot) form = qobject_cast<QFormLayout *>(cbRoot->parentWidget()->layout());

    /* Корневой коммутатор больше не выбирается в диалоге: после запуска
     * маршруты по умолчанию строятся от s1, а дальше пользователь кликами
     * по свичам добавляет деревья от нужных (ПКМ → «Подсветить маршруты»).
     * Поэтому строку выбора корня прячем всегда. */
    setRowVisible(form, rootRow_, false);
    setRowVisible(form, segRow_, isSeg);

    /* Вторая метрика и ограничения — только для LARAC (для CSP/MCP/MCOP
     * заранее доступны Ryu, но в C++ дефолтной локальной визуализации
     * нет). Лямбда — только для LARAC. */
    setRowVisible(form, metric2Row_, isLARAC);
    /* R₁ (ограничение по 1-й метрике) в классическом LARAC не используется —
     * минимизируем 1-ю метрику, ограничиваем только 2-ю (R₂). Прячем всегда,
     * чтобы не вводить в заблуждение. */
    setRowVisible(form, restr1Row_, false);
    setRowVisible(form, restr2Row_, isLARAC);
    setRowVisible(form, lambdaRow_, isLARAC);

    /* Подсказка с описанием — что делает алгоритм. */
    if (infoLabel)
    {
        if (isSeg)
        {
            infoLabel->setText(tr("Сегментирование разбивает топологию на K групп. "
                                  "Узлы каждого сегмента подсвечиваются своим цветом. "
                                  "Для активной маршрутизации (см. ниже) сегменты "
                                  "влияют на изоляцию трафика."));
        }
        else if (isLARAC)
        {
            infoLabel->setText(tr("LARAC — приближённый QoS-маршрут через "
                                  "лагранжеву релаксацию. Минимизирует сумму "
                                  "<i>m₁ + λ·m₂</i> при бюджете <i>R₁ + λ·R₂</i>. "
                                  "Подойдёт когда нужен компромисс между двумя "
                                  "метриками (например задержка vs потери)."));
        }
        else if (name == QStringLiteral("Парные переходы"))
        {
            infoLabel->setText(tr("Алгоритм парных переходов (глава 6 пособия): "
                                  "строит ДОМ от корня S₀ и заранее находит "
                                  "резервные парные каналы. При изменении метрики "
                                  "Ryu мгновенно переключит трафик на резерв "
                                  "вместо полного пересчёта."));
        }
        else
        {
            infoLabel->setText(tr("Дейкстра — кратчайшее дерево от корневого "
                                  "коммутатора по выбранной метрике. Результат — "
                                  "ДОМ красным; при активной маршрутизации "
                                  "OpenFlow-правила установятся для всех пар."));
        }
    }
}

void AlgorithmDialog::callRyu(const QString &endpoint)
{
    QUrl url(QStringLiteral("http://127.0.0.1:8080") + endpoint);
    QNetworkRequest req(url);
    QNetworkReply *reply = nam_->get(req);
    statusLabel->setStyleSheet("color:#475569; padding:4px;");
    statusLabel->setText(tr("→ запрос к Ryu: %1 …").arg(endpoint));
    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        const int code = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError)
        {
            statusLabel->setStyleSheet("color:#B91C1C; padding:4px;");
            statusLabel->setText(tr("Ryu %1: ошибка — %2").arg(endpoint,
                                                                reply->errorString()));
        }
        else
        {
            statusLabel->setStyleSheet("color:#0F766E; padding:4px;");
            statusLabel->setText(tr("Ryu %1: %2 — flow-правила обновлены")
                                 .arg(endpoint).arg(code));
            emit signalStatus(tr("Активная маршрутизация: %1 → %2")
                              .arg(endpoint).arg(code));
        }
        reply->deleteLater();
    });
}

void AlgorithmDialog::onRunClicked()
{
    if (cbRoot->count() == 0)
    {
        reject();
        return;
    }
    QString algId = cbAlgorithm->currentData().toString();
    QString algoName = cbAlgorithm->currentText();
    int rootIdx = cbRoot->currentData().toInt();
    QString metric = cbMetric->currentData().toString();

    /* Собираем дополнительные параметры — попадут и в Core (для C++
     * визуализации), и в URL Ryu (для активной маршрутизации). */
    QVariantMap params;
    if (algoName == QStringLiteral("Жадное сегментирование"))
    {
        params.insert("k", spinSegments_->value());
    }
    else if (algoName == QStringLiteral("LARAC"))
    {
        params.insert("metric1", metric);
        params.insert("metric2", cbMetric2_->currentData().toString());
        params.insert("restriction1", spinRestriction1_->value());
        params.insert("restriction2", spinRestriction2_->value());
        params.insert("lambd", spinLambda_->value());
    }

    emit signalRun(algId, rootIdx, metric);
    emit signalRunWithParams(algId, rootIdx, metric, params);

    /* Если включена активная маршрутизация — дёргаем Ryu /route/activate
     * с теми же параметрами. */
    if (chkActive->isChecked())
    {
        QUrlQuery q;
        q.addQueryItem("name", algoName);
        q.addQueryItem("metric", metric);
        for (auto it = params.cbegin(); it != params.cend(); ++it)
        {
            q.addQueryItem(it.key(), it.value().toString());
        }
        callRyu(QStringLiteral("/route/activate?") + q.toString(QUrl::FullyEncoded));
        /* НЕ закрываем — пусть пользователь увидит статус ответа. */
        return;
    }
    accept();
}

void AlgorithmDialog::onClearFlowsClicked()
{
    callRyu(QStringLiteral("/route/clear"));
    chkActive->setChecked(false);
}
