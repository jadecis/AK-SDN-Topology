#ifndef LINKDIALOG_H
#define LINKDIALOG_H

#include <QDialog>
#include "Link.h"

class SSLink;

namespace Ui {
class LinkDialog;
}

class LinkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LinkDialog(SSLink *link, QWidget *parent = 0);
    ~LinkDialog();

    SSLink * getLink() { return link; }

private:
    Ui::LinkDialog * ui;
    SSLink * link;

    void SetWindowProperties();

private slots:
    virtual void accept();
};

#endif // LINKDIALOG_H
