#include "ControllerDialog.h"
#include "SdnController.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QRegularExpressionValidator>
#include <QMessageBox>

ControllerDialog::ControllerDialog(SdnController *controller, QWidget *parent) :
    QDialog(parent),
    controller_(controller),
    edName_(nullptr),
    edIp_(nullptr),
    spPort_(nullptr),
    cbType_(nullptr)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("Контроллер %1").arg(controller_ ? controller_->getName() : "?"));
    setWindowIcon(ThemeManager::instance()->makeIcon(":/modern/modern/controller.svg",
                                                     QSize(48, 48)));
    setMinimumWidth(420);
    setSizeGripEnabled(true);
    buildUi();
}

void ControllerDialog::buildUi()
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(12);

    QGroupBox *box = new QGroupBox(tr("Параметры контроллера"), this);
    QFormLayout *form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(10);

    edName_ = new QLineEdit(box);
    edName_->setPlaceholderText("c0");
    edName_->setText(controller_ ? controller_->getName() : "c0");
    QRegularExpression nameRe("[a-zA-Z][a-zA-Z0-9_]{0,31}");
    edName_->setValidator(new QRegularExpressionValidator(nameRe, edName_));
    form->addRow(tr("Имя:"), edName_);

    edIp_ = new QLineEdit(box);
    edIp_->setPlaceholderText("127.0.0.1");
    edIp_->setText(controller_ ? controller_->getIp() : "127.0.0.1");
    QRegularExpression ipRe(
        "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"
        "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    edIp_->setValidator(new QRegularExpressionValidator(ipRe, edIp_));
    form->addRow(tr("IP-адрес:"), edIp_);

    spPort_ = new QSpinBox(box);
    spPort_->setRange(1, 65535);
    spPort_->setValue(controller_ ? controller_->getPort().toInt() : 6653);
    spPort_->setSuffix(tr("  (OF default: 6653)"));
    form->addRow(tr("Порт:"), spPort_);

    cbType_ = new QComboBox(box);
    cbType_->addItem(tr("Remote (Ryu, ONOS, OpenDaylight)"), "remote");
    cbType_->addItem(tr("Reference (Mininet встроенный)"), "reference");
    form->addRow(tr("Тип:"), cbType_);

    outer->addWidget(box);

    QLabel *hint = new QLabel(tr(
        "<i>Порт 6653 — стандартный для OpenFlow 1.3. "
        "Используйте 127.0.0.1 для локального Ryu.</i>"), this);
    hint->setStyleSheet("color:#6B7280; font-size: 9pt;");
    hint->setWordWrap(true);
    outer->addWidget(hint);

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();
    QPushButton *btnCancel = new QPushButton(tr("Отмена"), this);
    btnCancel->setStyleSheet("QPushButton { background:#E5E7EB; color:#111827; "
                             "border-radius:6px; padding:6px 16px; }"
                             "QPushButton:hover { background:#CBD5E1; }");
    QPushButton *btnOk = new QPushButton(tr("Сохранить"), this);
    btnOk->setDefault(true);
    btnOk->setStyleSheet("QPushButton { background:#2D6CDF; color:white; border:0; "
                         "border-radius:6px; padding:6px 16px; font-weight:600; }"
                         "QPushButton:hover { background:#1E4FB1; }");
    btns->addWidget(btnCancel);
    btns->addWidget(btnOk);
    outer->addLayout(btns);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, this, &ControllerDialog::onAccept);
}

void ControllerDialog::onAccept()
{
    if (!controller_) { accept(); return; }
    QString ip = edIp_->text().trimmed();
    if (ip.isEmpty() || !edIp_->hasAcceptableInput())
    {
        QMessageBox::warning(this, tr("Ошибка"), tr("Введите корректный IPv4-адрес."));
        edIp_->setFocus();
        return;
    }
    QString name = edName_->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("Ошибка"), tr("Имя контроллера не может быть пустым."));
        edName_->setFocus();
        return;
    }
    controller_->setName(name);
    controller_->setIp(ip);
    controller_->setPort(QString::number(spPort_->value()));
    accept();
}
