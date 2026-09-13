#ifndef TEXTLABELDIALOG_H
#define TEXTLABELDIALOG_H

#include <QDialog>

class TextLabel;

namespace Ui {
class TextLabelDialog;
}

class TextLabelDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit TextLabelDialog(TextLabel *textLabel, QWidget *parent = 0);
    ~TextLabelDialog();
    
private:
    Ui::TextLabelDialog *ui;
    TextLabel *textLabel;
    void setWindowProperties();

private slots:
    virtual void accept();
};

#endif // TEXTLABELDIALOG_H
