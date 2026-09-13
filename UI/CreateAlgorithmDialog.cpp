#include "CreateAlgorithmDialog.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QMessageBox>
#include <QRegularExpression>

CreateAlgorithmDialog::CreateAlgorithmDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Создать пользовательский алгоритм"));
    setWindowIcon(ThemeManager::instance()->makeIcon(":/modern/modern/algorithm.svg",
                                                     QSize(48, 48)));
    setMinimumWidth(520);
    setSizeGripEnabled(true);

    QVBoxLayout *outer = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr(
        "Создаётся новый файл в папке <code>ryu/algorithms_user/</code> "
        "с шаблоном класса, наследующего <code>BaseAlgorithm</code>. "
        "После сохранения откройте веб-интерфейс контроллера и нажмите "
        "<b>Перезагрузить алгоритмы</b>, либо перезапустите Ryu."), this);
    intro->setWordWrap(true);
    intro->setStyleSheet("color:#6B7280;");
    outer->addWidget(intro);

    QGroupBox *box = new QGroupBox(tr("Параметры"), this);
    QFormLayout *form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight);

    algoNameEdit_ = new QLineEdit(tr("Мой алгоритм"), box);
    form->addRow(tr("Имя алгоритма:"), algoNameEdit_);

    fileNameEdit_ = new QLineEdit("my_algorithm.py", box);
    form->addRow(tr("Имя файла:"), fileNameEdit_);

    classNameEdit_ = new QLineEdit("MyAlgorithm", box);
    form->addRow(tr("Имя класса:"), classNameEdit_);

    outer->addWidget(box);

    previewLbl_ = new QLabel(this);
    previewLbl_->setWordWrap(true);
    previewLbl_->setStyleSheet("background:#F4F6FA; padding:8px; border-radius:6px;"
                               " font-family: 'JetBrains Mono', monospace; font-size: 9pt;");
    outer->addWidget(previewLbl_);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *btnCancel = new QPushButton(tr("Отмена"), this);
    btnCancel->setStyleSheet("QPushButton { background:#E5E7EB; color:#111827; }"
                             "QPushButton:hover { background:#CBD5E1; }");
    QPushButton *btnOk = new QPushButton(tr("Создать и открыть"), this);
    btnOk->setDefault(true);
    buttons->addWidget(btnCancel);
    buttons->addWidget(btnOk);
    outer->addLayout(buttons);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, this, &CreateAlgorithmDialog::onAccept);
    connect(algoNameEdit_, &QLineEdit::textChanged,
            this, &CreateAlgorithmDialog::onAlgoNameChanged);

    onAlgoNameChanged(algoNameEdit_->text());
}

QString CreateAlgorithmDialog::toClassName(const QString &humanName)
{
    /* CamelCase: разделяем по пробелу/-/_, делаем первую букву каждого слова заглавной. */
    QStringList parts = humanName.split(QRegularExpression("[\\s_\\-]+"),
                                        QString::SkipEmptyParts);
    QString result;
    for (const QString &p : parts)
    {
        if (p.isEmpty()) continue;
        QString clean;
        for (QChar c : p)
        {
            if (c.isLetterOrNumber()) clean += c;
        }
        if (!clean.isEmpty())
        {
            clean[0] = clean[0].toUpper();
            result += clean;
        }
    }
    if (result.isEmpty() || !result[0].isLetter())
        result = "MyAlgorithm" + result;
    return result;
}

QString CreateAlgorithmDialog::toFileName(const QString &humanName)
{
    /* snake_case.py */
    QString lower = humanName.toLower();
    QString out;
    bool prevSep = false;
    for (QChar c : lower)
    {
        if (c.isLetterOrNumber())
        {
            out += c;
            prevSep = false;
        }
        else if (!prevSep && !out.isEmpty())
        {
            out += "_";
            prevSep = true;
        }
    }
    while (out.endsWith("_")) out.chop(1);
    if (out.isEmpty()) out = "my_algorithm";
    return out + ".py";
}

void CreateAlgorithmDialog::onAlgoNameChanged(const QString &text)
{
    fileNameEdit_->setText(toFileName(text));
    classNameEdit_->setText(toClassName(text));
    QString preview = QString(
        "from base_algorithm import BaseAlgorithm\n\n"
        "class %1(BaseAlgorithm):\n"
        "    name = \"%2\"\n"
        "    params = { \"metric\": (\"str\", \"delay\", \"...\") }\n\n"
        "    def run(self, src, dst, graphs, params):\n"
        "        self.log(\"Запуск\")\n"
        "        return { \"routes\": [], \"cost\": 0 }"
    ).arg(classNameEdit_->text()).arg(algoNameEdit_->text());
    previewLbl_->setText("<pre style='margin:0'>" + preview.toHtmlEscaped() + "</pre>");
}

QString CreateAlgorithmDialog::algorithmName() const { return algoNameEdit_->text().trimmed(); }
QString CreateAlgorithmDialog::fileName() const { return fileNameEdit_->text().trimmed(); }
QString CreateAlgorithmDialog::className() const { return classNameEdit_->text().trimmed(); }

void CreateAlgorithmDialog::onAccept()
{
    if (algorithmName().isEmpty())
    {
        QMessageBox::warning(this, tr("Ошибка"), tr("Имя алгоритма не может быть пустым."));
        return;
    }
    if (fileName().isEmpty() || !fileName().endsWith(".py"))
    {
        QMessageBox::warning(this, tr("Ошибка"), tr("Имя файла должно заканчиваться на .py"));
        return;
    }
    if (className().isEmpty() || !className()[0].isLetter())
    {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Имя класса должно начинаться с буквы и быть валидным Python-идентификатором."));
        return;
    }
    accept();
}
