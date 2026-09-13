#ifndef HOSTDIALOG_H
#define HOSTDIALOG_H

#include <QDialog>
#include "Host.h"

namespace Ui {
class HostDialog;
}

class HostDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit HostDialog(Host*, QWidget *parent = 0);
    ~HostDialog();

    Host * getHost() { return host; }
    
private:
    Host* host;
    Ui::HostDialog *ui;

    void SetWindowProperties();

private slots:
    virtual void accept();
};

#endif // HOSTDIALOG_H




