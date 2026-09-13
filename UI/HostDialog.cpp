#include "HostDialog.h"
#include "ui_HostDialog.h"

HostDialog::HostDialog(Host *host, QWidget *parent) :
    QDialog(parent), host(host), ui(new Ui::HostDialog)
{
    ui->setupUi(this);
    SetWindowProperties();
}

HostDialog::~HostDialog()
{
    delete ui;
}

void HostDialog::SetWindowProperties()
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    this->setWindowTitle(QString("Host %0").arg(host->getName()));
    ui->txtIpAddress->setText(host->getIp());
    ui->txtMacAddress->setText(host->getMac());
}

void HostDialog::accept()
{
    host->setIp(ui->txtIpAddress->text());
    host->setMac(ui->txtMacAddress->text());
    done(QDialog::Accepted);
}
