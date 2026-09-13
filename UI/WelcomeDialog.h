#ifndef WELCOMEDIALOG_H
#define WELCOMEDIALOG_H

#include <QDialog>
#include <QString>

class QListWidget;

/* WelcomeDialog — стартовое окно при запуске.
 * Варианты:
 *   • Создать новый проект → emits requestCreateNewProject
 *   • Открыть существующий → emits requestOpenProject(QString dir)
 *   • Выбрать из последних → emits requestOpenProject(dir)
 *   • Закрыть и работать без проекта */
class WelcomeDialog : public QDialog
{
    Q_OBJECT
public:
    enum Action {
        ActionNone,
        ActionNewProject,
        ActionOpenProject,
        ActionSkip
    };

    explicit WelcomeDialog(QWidget *parent = nullptr);

    Action chosenAction() const { return action_; }
    QString chosenProjectDir() const { return chosenDir_; }

private slots:
    void onNewProject();
    void onOpenProject();
    void onOpenRecent();
    void onSkip();

private:
    QListWidget *recentList_;
    Action action_;
    QString chosenDir_;
    void populateRecent();
};

#endif // WELCOMEDIALOG_H
