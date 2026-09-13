#ifndef NEWPROJECTDIALOG_H
#define NEWPROJECTDIALOG_H

#include <QDialog>
#include "ProjectManager.h"

class QLineEdit;
class QComboBox;
class QLabel;

/* NewProjectDialog — мастер создания нового проекта.
 * Поля:
 *   • Имя проекта (по умолчанию sdn-topology-1)
 *   • Папка-родитель (по умолчанию ~/SnetProjects)
 *   • Шаблон (Empty / Ring / Tree)
 *
 * При accept() — поля доступны через getters. Сам ProjectManager
 * вызывается на стороне Core. */
class NewProjectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewProjectDialog(QWidget *parent = nullptr);

    QString projectName() const;
    QString parentDirectory() const;
    QString fullProjectDir() const;        // parentDir + name
    ProjectManager::Template chosenTemplate() const;

private slots:
    void onBrowse();
    void onAccept();
    void updatePreview();

private:
    QLineEdit *nameEdit_;
    QLineEdit *dirEdit_;
    QComboBox *templateCombo_;
    QLabel *previewLbl_;
};

#endif // NEWPROJECTDIALOG_H
