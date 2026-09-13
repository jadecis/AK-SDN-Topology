#include "NewProjectDialog.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QFileInfo>
#include <QStandardPaths>

NewProjectDialog::NewProjectDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Новый проект"));
    setWindowIcon(ThemeManager::instance()->makeIcon(":/modern/modern/new.svg",
                                                     QSize(48, 48)));
    setMinimumWidth(540);
    setSizeGripEnabled(true);

    QVBoxLayout *outer = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr("Создайте проект SDN-топологии. Каталог проекта будет содержать "
                                  "файлы топологии, Mininet-скрипт, матрицу метрик и стартовый "
                                  "Ryu-контроллер."), this);
    intro->setWordWrap(true);
    intro->setStyleSheet("color: #6B7280;");
    outer->addWidget(intro);

    QGroupBox *box = new QGroupBox(tr("Параметры проекта"), this);
    QFormLayout *form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight);

    nameEdit_ = new QLineEdit("sdn-topology-1", box);
    form->addRow(tr("Имя проекта:"), nameEdit_);

    QString defaultParent = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (defaultParent.isEmpty()) defaultParent = QDir::homePath();
    defaultParent = QDir(defaultParent).filePath("SnetProjects");

    QWidget *dirRow = new QWidget(box);
    QHBoxLayout *dirLay = new QHBoxLayout(dirRow);
    dirLay->setContentsMargins(0, 0, 0, 0);
    dirEdit_ = new QLineEdit(defaultParent, dirRow);
    QPushButton *btnBrowse = new QPushButton(tr("Обзор…"), dirRow);
    btnBrowse->setStyleSheet("QPushButton { background:#E5E7EB; color:#111827; }"
                             "QPushButton:hover { background:#CBD5E1; }");
    dirLay->addWidget(dirEdit_, 1);
    dirLay->addWidget(btnBrowse);
    form->addRow(tr("Папка-родитель:"), dirRow);

    templateCombo_ = new QComboBox(box);
    /* Шаблоны выстроены от простого к сложному. Подсказка под комбобоксом
     * меняется в updatePreview() и поясняет где какой алгоритм
     * демонстрируется. */
    templateCombo_->addItem(tr("Пустой (только контроллер c0)"),
                            int(ProjectManager::Empty));
    templateCombo_->addItem(tr("Звезда (1 свитч + 4 хоста)"),
                            int(ProjectManager::Star));
    templateCombo_->addItem(tr("Линейная (4 свитча + 4 хоста)"),
                            int(ProjectManager::LinearFour));
    templateCombo_->addItem(tr("Кольцо-5 (5 свитчей + 5 хостов) — парные переходы"),
                            int(ProjectManager::RingFive));
    templateCombo_->addItem(tr("Дерево-7 (7 свитчей + 8 хостов) — сегментирование"),
                            int(ProjectManager::TreeSeven));
    templateCombo_->addItem(tr("Mesh + Docker (6 свитчей + 4 хоста + 2 контейнера) — активная маршрутизация"),
                            int(ProjectManager::MeshDocker));
    templateCombo_->setCurrentIndex(3);  // RingFive — наглядное демо
    form->addRow(tr("Шаблон топологии:"), templateCombo_);

    outer->addWidget(box);

    previewLbl_ = new QLabel(this);
    previewLbl_->setWordWrap(true);
    previewLbl_->setStyleSheet("padding: 8px; background:#F4F6FA; border-radius:6px; color:#374151;");
    outer->addWidget(previewLbl_);

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    QPushButton *btnCancel = new QPushButton(tr("Отмена"), this);
    btnCancel->setStyleSheet("QPushButton { background:#E5E7EB; color:#111827; }"
                             "QPushButton:hover { background:#CBD5E1; }");
    QPushButton *btnOk = new QPushButton(tr("Создать"), this);
    btnOk->setDefault(true);
    buttons->addWidget(btnCancel);
    buttons->addWidget(btnOk);
    outer->addLayout(buttons);

    connect(btnBrowse, &QPushButton::clicked, this, &NewProjectDialog::onBrowse);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, this, &NewProjectDialog::onAccept);
    connect(nameEdit_, &QLineEdit::textChanged, this, &NewProjectDialog::updatePreview);
    connect(dirEdit_, &QLineEdit::textChanged, this, &NewProjectDialog::updatePreview);
    connect(templateCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NewProjectDialog::updatePreview);
    updatePreview();
}

void NewProjectDialog::onBrowse()
{
    QString d = QFileDialog::getExistingDirectory(this, tr("Выберите папку-родитель"),
                                                  dirEdit_->text());
    if (!d.isEmpty())
        dirEdit_->setText(d);
}

QString NewProjectDialog::projectName() const { return nameEdit_->text().trimmed(); }
QString NewProjectDialog::parentDirectory() const { return dirEdit_->text().trimmed(); }
QString NewProjectDialog::fullProjectDir() const
{
    return QDir(parentDirectory()).filePath(projectName());
}
ProjectManager::Template NewProjectDialog::chosenTemplate() const
{
    return static_cast<ProjectManager::Template>(templateCombo_->currentData().toInt());
}

void NewProjectDialog::updatePreview()
{
    QString full = fullProjectDir();
    /* Описание выбранного шаблона — что в нём, и какие алгоритмы на нём
     * имеют смысл демонстрировать. */
    QString tmplDesc;
    switch (chosenTemplate())
    {
    case ProjectManager::Empty:
        tmplDesc = tr("<b>Пустой</b> — только контроллер. Добавьте узлы и каналы вручную.");
        break;
    case ProjectManager::Star:
        tmplDesc = tr("<b>Звезда</b> — 1 коммутатор в центре, 4 хоста по краям. "
                      "Простейший случай для проверки <i>pingall</i> и L2-обучения.");
        break;
    case ProjectManager::LinearFour:
        tmplDesc = tr("<b>Линейная</b> — 4 коммутатора в цепочке + 4 хоста. "
                      "Используйте для демонстрации <i>Дейкстры</i> (путь однозначный) "
                      "и QoS-маршрутизации.");
        break;
    case ProjectManager::RingFive:
        tmplDesc = tr("<b>Кольцо-5</b> — замкнутое кольцо 5 коммутаторов, на каждом по хосту, "
                      "разные задержки рёбер. Идеально для <i>алгоритма парных переходов</i>: "
                      "меняешь метрику одного канала — Ryu переключает трафик на резерв.");
        break;
    case ProjectManager::TreeSeven:
        tmplDesc = tr("<b>Дерево-7</b> — двоичное дерево 7 коммутаторов + 8 хостов. "
                      "Подходит для <i>сегментирования</i> (явные поддеревья-кластеры) "
                      "и сравнения алгоритмов.");
        break;
    case ProjectManager::MeshDocker:
        tmplDesc = tr("<b>Mesh + Docker</b> — 6 коммутаторов кольцом с диагоналями, "
                      "4 хоста-клиента и 2 Docker-контейнера (postgres + nginx). "
                      "Крупная сеть для <i>активной маршрутизации</i>: запускайте алгоритм "
                      "с галочкой «Установить flow-правила» и наблюдайте, как меняются "
                      "реальные пути запросов.");
        break;
    case ProjectManager::Ring:
        tmplDesc = tr("<i>Кольцо</i> (LEGACY — 3 свитча). Сохранён для совместимости.");
        break;
    case ProjectManager::Tree:
        tmplDesc = tr("<i>Дерево</i> (LEGACY — 3 свитча). Сохранён для совместимости.");
        break;
    }

    previewLbl_->setText(tr("Будет создан каталог:\n  <b>%1</b>\n\n%2\n\n"
                            "Содержимое каталога:\n"
                            "  • topology.sdn.xml\n"
                            "  • topology.sdn.py\n"
                            "  • metrics.txt\n"
                            "  • ryu/ — шаблоны контроллера\n"
                            "  • README.md").arg(full, tmplDesc));
}

void NewProjectDialog::onAccept()
{
    if (projectName().isEmpty())
    {
        QMessageBox::warning(this, tr("Ошибка"), tr("Имя проекта не может быть пустым."));
        return;
    }
    QString name = projectName();
    if (name.contains('/') || name.contains('\\') || name.startsWith('.'))
    {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Имя проекта содержит недопустимые символы."));
        return;
    }
    QString full = fullProjectDir();
    if (QFileInfo::exists(full))
    {
        QMessageBox::StandardButton ans = QMessageBox::question(this, tr("Каталог существует"),
            tr("Каталог уже существует:\n%1\n\nИспользовать его? "
               "(существующие файлы будут перезаписаны)").arg(full),
            QMessageBox::Yes | QMessageBox::No);
        if (ans != QMessageBox::Yes) return;
    }
    accept();
}
