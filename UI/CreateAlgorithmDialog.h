#ifndef CREATEALGORITHMDIALOG_H
#define CREATEALGORITHMDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;

/* CreateAlgorithmDialog — мастер создания пользовательского алгоритма.
 * Поля:
 *   • Имя алгоритма (отображаемое в веб-интерфейсе)
 *   • Имя файла (.py) — авто-предлагается из имени алгоритма
 *   • Имя класса — авто-предлагается из имени алгоритма
 *
 * Возвращает данные через геттеры. Создание файла делает Core. */
class CreateAlgorithmDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CreateAlgorithmDialog(QWidget *parent = nullptr);

    QString algorithmName() const;
    QString fileName() const;
    QString className() const;

private slots:
    void onAlgoNameChanged(const QString &text);
    void onAccept();

private:
    QLineEdit *algoNameEdit_;
    QLineEdit *fileNameEdit_;
    QLineEdit *classNameEdit_;
    QLabel *previewLbl_;

    static QString toClassName(const QString &humanName);
    static QString toFileName(const QString &humanName);
};

#endif // CREATEALGORITHMDIALOG_H
