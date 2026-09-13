#include "WelcomeDialog.h"
#include "ProjectManager.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

WelcomeDialog::WelcomeDialog(QWidget *parent) :
    QDialog(parent),
    action_(ActionNone)
{
    setWindowTitle(tr("Добро пожаловать — SDN Topology"));
    setWindowIcon(ThemeManager::instance()->makeIcon(":/modern/modern/app.svg",
                                                     QSize(64, 64)));
    setMinimumSize(720, 460);
    setSizeGripEnabled(true);

    QHBoxLayout *outer = new QHBoxLayout(this);

    /* Левая колонка — большие кнопки действий */
    QVBoxLayout *left = new QVBoxLayout();
    left->setSpacing(12);

    QLabel *title = new QLabel(tr("<h2 style='margin:0'>SDN Topology</h2>"
                                   "<p style='color:#6B7280;margin:0'>"
                                   "Редактор программно-конфигурируемых сетей</p>"), this);
    left->addWidget(title);
    left->addSpacing(8);

    auto makeBigButton = [this](const QString &iconRes,
                                const QString &title,
                                const QString &subtitle) -> QPushButton * {
        QPushButton *btn = new QPushButton(this);
        btn->setIcon(ThemeManager::instance()->makeIcon(iconRes,
                     ThemeManager::instance()->accentColor(), QSize(28, 28)));
        btn->setIconSize(QSize(28, 28));
        btn->setText(QString("  %1\n  %2").arg(title).arg(subtitle));
        btn->setStyleSheet(
            "QPushButton {"
            "  text-align: left;"
            "  padding: 12px;"
            "  border-radius: 8px;"
            "  background:#F4F6FA;"
            "  color:#111827;"
            "  border: 1px solid #E5E7EB;"
            "}"
            "QPushButton:hover { background:#E5E7EB; }"
        );
        btn->setMinimumHeight(64);
        return btn;
    };

    QPushButton *btnNew = makeBigButton(":/modern/modern/new.svg",
        tr("Создать новый проект"),
        tr("Каталог + topology.sdn.xml + controller.py + Mininet-скрипт"));
    QPushButton *btnOpen = makeBigButton(":/modern/modern/open.svg",
        tr("Открыть проект из папки"),
        tr("Выбрать каталог с файлом topology.sdn.xml"));
    QPushButton *btnSkip = makeBigButton(":/modern/modern/edit.svg",
        tr("Работать без проекта"),
        tr("Пустой холст, потом можно сохранить как проект"));

    left->addWidget(btnNew);
    left->addWidget(btnOpen);
    left->addWidget(btnSkip);
    left->addStretch();

    QLabel *hint = new QLabel(tr("Подсказка: используйте Ctrl+T для переключения темы."), this);
    hint->setStyleSheet("color:#9CA3AF; font-size:9pt;");
    left->addWidget(hint);

    outer->addLayout(left, 1);

    /* Правая колонка — список последних проектов */
    QVBoxLayout *right = new QVBoxLayout();
    QLabel *rtitle = new QLabel(tr("<b>Последние проекты</b>"), this);
    right->addWidget(rtitle);

    recentList_ = new QListWidget(this);
    recentList_->setStyleSheet("QListWidget { background:#FFFFFF; border:1px solid #E5E7EB; "
                                "border-radius:8px; padding:4px; }"
                                "QListWidget::item { padding: 6px 8px; border-radius: 4px; }"
                                "QListWidget::item:hover { background:#F4F6FA; }"
                                "QListWidget::item:selected { background:#2D6CDF; color:white; }");
    recentList_->setMinimumWidth(280);
    right->addWidget(recentList_, 1);

    populateRecent();

    outer->addLayout(right, 1);

    connect(btnNew, &QPushButton::clicked, this, &WelcomeDialog::onNewProject);
    connect(btnOpen, &QPushButton::clicked, this, &WelcomeDialog::onOpenProject);
    connect(btnSkip, &QPushButton::clicked, this, &WelcomeDialog::onSkip);
    connect(recentList_, &QListWidget::itemDoubleClicked,
            this, &WelcomeDialog::onOpenRecent);
}

void WelcomeDialog::populateRecent()
{
    recentList_->clear();
    /* Отбрасываем записи о проектах, которых уже нет на диске —
     * пользователь просил не показывать «(не найден)» строки.
     * Сразу же чистим запись в настройках, чтобы список не пух. */
    QStringList recents;
    for (const QString &dir : ProjectManager::recentProjects())
    {
        if (QFileInfo::exists(dir)) recents << dir;
        else ProjectManager::removeRecentProject(dir);
    }
    if (recents.isEmpty())
    {
        QListWidgetItem *empty = new QListWidgetItem(tr("(нет последних проектов)"));
        empty->setFlags(Qt::NoItemFlags);
        empty->setForeground(QColor("#9CA3AF"));
        recentList_->addItem(empty);
        return;
    }
    for (const QString &dir : recents)
    {
        QFileInfo info(dir);
        QString name = info.fileName().isEmpty() ? dir : info.fileName();
        QListWidgetItem *it = new QListWidgetItem(
            QString("%1\n%2").arg(name).arg(dir));
        it->setData(Qt::UserRole, dir);
        recentList_->addItem(it);
    }
}

void WelcomeDialog::onNewProject()
{
    action_ = ActionNewProject;
    accept();
}

void WelcomeDialog::onOpenProject()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Откройте папку проекта"),
                                                    QDir::homePath());
    if (dir.isEmpty()) return;
    chosenDir_ = dir;
    action_ = ActionOpenProject;
    accept();
}

void WelcomeDialog::onOpenRecent()
{
    auto *it = recentList_->currentItem();
    if (!it) return;
    QString dir = it->data(Qt::UserRole).toString();
    if (dir.isEmpty() || !QFileInfo::exists(dir)) return;
    chosenDir_ = dir;
    action_ = ActionOpenProject;
    accept();
}

void WelcomeDialog::onSkip()
{
    action_ = ActionSkip;
    accept();
}
