#ifndef DOCKERDIALOG_H
#define DOCKERDIALOG_H

#include <QDialog>

class DockerNode;
class QLineEdit;
class QPlainTextEdit;
class QComboBox;

/* DockerDialog — программный диалог свойств Docker-узла.
 *
 * Поля:
 *   name        — имя контейнера (только чтение, генерируется)
 *   image       — Docker-образ (combo с пресетами + ручной ввод)
 *   ip / mac    — сетевые параметры
 *   command     — переопределение CMD из образа
 *   environment — список KEY=VALUE
 *   volumes     — список host:container
 *   ports       — список host:container портов для проброса
 */
class QLabel;

class DockerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DockerDialog(DockerNode *node, QWidget *parent = nullptr);

protected:
    void accept() override;

private slots:
    void applyPreset(int index);

private:
    DockerNode *node_;
    QComboBox *presetBox_;
    QLineEdit *nameEdit_;
    QComboBox *imageBox_;
    QLineEdit *ipEdit_;
    QLineEdit *macEdit_;
    QLineEdit *commandEdit_;
    QPlainTextEdit *envEdit_;
    QPlainTextEdit *volumesEdit_;
    QPlainTextEdit *portsEdit_;
    QLabel *presetHint_;

    void buildUi();
    void loadFromNode();
};

#endif // DOCKERDIALOG_H
