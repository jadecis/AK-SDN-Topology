#include "LinkDialog.h"
#include "ui_LinkDialog.h"
#include "SSLink.h"
#include "Node.h"

LinkDialog::LinkDialog(SSLink *link, QWidget *parent) :
    QDialog(parent), ui(new Ui::LinkDialog), link(link)
{
    ui->setupUi(this);
    SetWindowProperties();
}

LinkDialog::~LinkDialog()
{
    delete ui;
}

void LinkDialog::SetWindowProperties()
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    this->setWindowTitle(QString("Link %0 - %1")
                         .arg(link->getNode1()->getName())
                         .arg(link->getNode2()->getName()));
    ui->txtBandwidth->setText(QString::number(link->getBandwidth()));
    ui->txtDelay->setText(QString::number(link->getDelay()));
    ui->txtPacketLoss->setText(QString::number(link->getPacketLoss()));
}

void LinkDialog::accept()
{
    link->setBandwidth(ui->txtBandwidth->text().toFloat());
    link->setDelay(ui->txtDelay->text().toFloat());
    link->setPacketLossRate(ui->txtPacketLoss->text().toFloat());
    done(QDialog::Accepted);
}
