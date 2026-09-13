#include "TextLabelDialog.h"
#include "ui_TextLabelDialog.h"
#include "TextLabel.h"

#include <QPushButton>

TextLabelDialog::TextLabelDialog(TextLabel *textLabel, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TextLabelDialog),
    textLabel(textLabel)
{
    ui->setupUi(this);
    setWindowProperties();
}

TextLabelDialog::~TextLabelDialog()
{
    delete ui;
}

void TextLabelDialog::setWindowProperties()
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("Текстовая метка"));
    ui->txtTextArea->setText(textLabel->getContent());
    ui->txtTextArea->setFont(QFont("sans-serif", textLabel->getFontSize()));
    ui->txtTextArea->setFocus();
    ui->txtTextArea->selectAll();
    ui->spinFontSize->setValue(textLabel->getFontSize());

    /* Стандартные QDialogButtonBox::Ok / Cancel автоматически
     * подписываются как «OK» / «Cancel» (в зависимости от локали Qt).
     * Принудительно ставим русские подписи. */
    if (auto *okBtn = ui->buttonBox->button(QDialogButtonBox::Ok))
        okBtn->setText(tr("Сохранить"));
    if (auto *cancelBtn = ui->buttonBox->button(QDialogButtonBox::Cancel))
        cancelBtn->setText(tr("Отмена"));

    /* Шрифт в поле ввода меняется вместе со spinbox — пользователь
     * сразу видит результат до сохранения. */
    connect(ui->spinFontSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int pt) {
        QFont f = ui->txtTextArea->font();
        f.setPointSize(pt);
        ui->txtTextArea->setFont(f);
    });
}

void TextLabelDialog::accept()
{
    textLabel->setContent(ui->txtTextArea->toPlainText());
    textLabel->setFontSize(ui->spinFontSize->value());
    done(QDialog::Accepted);
}
