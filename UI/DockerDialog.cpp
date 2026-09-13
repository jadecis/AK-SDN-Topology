#include "DockerDialog.h"
#include "DockerNode.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QScreen>
#include <QGuiApplication>

/* Пресеты "под ключ" для демонстрации запросов с хоста / mininet-хоста к
 * контейнеру. Каждый пресет задаёт: image, command, env, проброс портов
 * на физический хост и подсказку как проверить сервис. Индекс 0 — пустой
 * (no-op), чтобы пользователь мог сбросить выбор без потери своих правок. */
struct DockerPreset {
    QString title;
    QString image;
    QString command;
    QStringList env;
    QStringList ports;     // host:container
    QString testHint;      // HTML-подсказка для пользователя
};

/* Хосты-порты ВСЕХ пресетов подобраны вне занятых SNet/Ryu:
 *   8080  — Ryu wsapi (REST/WebUI)
 *   5555  — SNet topology HTTP
 *   6633/6653 — OpenFlow
 * Внутри контейнера остаются стандартные порты сервиса (80, 5432, ...).
 *
 * КОМАНДА (dcmd) обязательна для всех сервисных образов: Containernet в
 * /usr/local/lib/python3.8/dist-packages/mininet/node.py:745 жёстко
 * подменяет CMD контейнера на /bin/bash, если dcmd не задан. Тогда
 * ENTRYPOINT образа НЕ запускается, и сервис (postgres, mysql, nginx, ...)
 * никогда не стартует — пинг есть, а порт сервиса closed/refused.
 * Поэтому в каждом пресете явно прописываем оригинальную пару
 * ENTRYPOINT + CMD из официального Dockerfile. */
static const QList<DockerPreset> &presets()
{
    static const QList<DockerPreset> kList = {
        {
            QObject::tr("— Свой / без пресета —"),
            "", "", {}, {}, ""
        },
        {
            QObject::tr("Веб: Nginx → http://localhost:8088"),
            "nginx",
            "/docker-entrypoint.sh nginx -g 'daemon off;'",
            {}, {"8088:80"},
            QObject::tr(
                "Откройте в браузере: <a href=\"http://localhost:8088\">http://localhost:8088</a> "
                "— увидите стартовую страницу nginx.<br>"
                "Из терминала: <code>curl http://localhost:8088</code> "
                "или <code>sudo docker exec mn.%2 curl -s http://localhost</code>")
        },
        {
            QObject::tr("Веб: Python http.server → http://localhost:8000"),
            "python:3.11-slim", "python3 -m http.server 8000",
            {}, {"8000:8000"},
            QObject::tr(
                "Откройте в браузере: <a href=\"http://localhost:8000\">http://localhost:8000</a> "
                "— листинг файлов внутри контейнера.<br>"
                "Из терминала: <code>curl http://localhost:8000</code>")
        },
        {
            QObject::tr("БД: PostgreSQL 16 → localhost:15432"),
            "postgres:16",
            "docker-entrypoint.sh postgres",
            {"POSTGRES_PASSWORD=secret", "POSTGRES_DB=demo", "POSTGRES_USER=demo"},
            {"15432:5432"},
            QObject::tr(
                "Подключение: <code>psql -h localhost -p 15432 -U demo -d demo</code> "
                "(пароль <code>secret</code>).<br>"
                "Из h1 в Mininet CLI: "
                "<code>h1 PGPASSWORD=secret psql -h %1 -U demo -d demo</code><br>"
                "Без psql на хосте: "
                "<code>sudo docker exec -it mn.%2 psql -U demo -d demo</code>")
        },
        {
            QObject::tr("БД: MySQL 8 → localhost:13306"),
            "mysql:8",
            "docker-entrypoint.sh mysqld",
            {"MYSQL_ROOT_PASSWORD=secret", "MYSQL_DATABASE=demo"},
            {"13306:3306"},
            QObject::tr(
                "Подключение: <code>mysql -h 127.0.0.1 -P 13306 -uroot -psecret demo</code><br>"
                "Из h1 в Mininet CLI: "
                "<code>h1 mysql -h %1 -uroot -psecret demo</code><br>"
                "Без mysql-клиента на хосте: "
                "<code>sudo docker exec -it mn.%2 mysql -uroot -psecret demo</code>")
        },
    };
    return kList;
}

DockerDialog::DockerDialog(DockerNode *node, QWidget *parent) :
    QDialog(parent), node_(node)
{
    setWindowTitle(tr("Docker-контейнер: %1").arg(node->getName()));
    setMinimumWidth(680);
    setMinimumHeight(560);
    /* Ограничиваем высоту диалога 80% экрана, чтобы при длинной подсказке
     * пресета OK/Cancel не уезжали за нижний край. Контент будет крутиться
     * в QScrollArea (см. buildUi). */
    if (QScreen *scr = QGuiApplication::primaryScreen())
    {
        int h = scr->availableGeometry().height();
        setMaximumHeight(qMax(560, int(h * 0.85)));
    }
    buildUi();
    loadFromNode();
}

void DockerDialog::buildUi()
{
    /* Корневой layout диалога. В нём ДВА элемента: QScrollArea с
     * содержимым (растягивается) и QDialogButtonBox с OK/Cancel
     * (фиксирован у нижнего края — никогда не уезжает за пределы окна). */
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll, 1);

    auto *content = new QWidget(scroll);
    scroll->setWidget(content);
    auto *root = new QVBoxLayout(content);

    /* Пресет — заполняет все остальные поля под типовой сервис. */
    auto *presetGroup = new QGroupBox(tr("Быстрый пресет"), content);
    auto *presetLayout = new QVBoxLayout(presetGroup);
    presetBox_ = new QComboBox(presetGroup);
    for (const auto &p : presets()) presetBox_->addItem(p.title);
    presetLayout->addWidget(presetBox_);
    presetHint_ = new QLabel(presetGroup);
    presetHint_->setWordWrap(true);
    presetHint_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    presetHint_->setStyleSheet("color: #94A3B8; padding: 4px;");
    presetHint_->setVisible(false);
    presetLayout->addWidget(presetHint_);
    connect(presetBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DockerDialog::applyPreset);
    root->addWidget(presetGroup);

    /* Основные параметры — форма с заметными отступами и единообразной
     * высотой полей. Без этого тема light.qss даёт padding 4px 8px на
     * QLineEdit, и общая высота поля 24px — строки кажутся слипшимися. */
    auto *mainGroup = new QGroupBox(tr("Основные параметры"), content);
    auto *form = new QFormLayout(mainGroup);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignTop | Qt::AlignLeft);
    form->setContentsMargins(16, 16, 16, 16);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(14);

    /* Единая высота, чтобы QLineEdit и QComboBox были на одной линии. */
    const int kFieldHeight = 34;

    nameEdit_ = new QLineEdit(mainGroup);
    nameEdit_->setReadOnly(true);
    nameEdit_->setMinimumHeight(kFieldHeight);
    form->addRow(tr("Имя:"), nameEdit_);

    imageBox_ = new QComboBox(mainGroup);
    imageBox_->setEditable(true);
    imageBox_->setMinimumHeight(kFieldHeight);
    imageBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    /* Containernet требует bash в контейнере (он запускает `bash --norc -is`
     * для приёма команд от Mininet CLI). Образы помеченные [no-bash]
     * требуют дополнительной установки bash или своего Dockerfile. */
    imageBox_->addItems({
        "ubuntu:22.04",            // bash есть
        "ubuntu:24.04",            // bash есть
        "debian:bookworm-slim",    // bash есть
        "python:3.11-slim",        // bash есть, удобно для своего приложения
        "nginx",                   // bash есть (не nginx:alpine — там нет!)
        "mysql:8",                 // bash есть
        "postgres:16",             // bash есть
        "redis",                   // bash есть
        "node:20",                 // bash есть
        // С bash нет — для них в команде используйте /bin/sh:
        "alpine:latest",
        "nginx:alpine",
        "redis:alpine"
    });
    form->addRow(tr("Образ:"), imageBox_);

    ipEdit_ = new QLineEdit(mainGroup);
    ipEdit_->setMinimumHeight(kFieldHeight);
    form->addRow(tr("IP-адрес:"), ipEdit_);

    macEdit_ = new QLineEdit(mainGroup);
    macEdit_->setMinimumHeight(kFieldHeight);
    form->addRow(tr("MAC:"), macEdit_);

    commandEdit_ = new QLineEdit(mainGroup);
    commandEdit_->setPlaceholderText(tr("Пусто = CMD/ENTRYPOINT из образа"));
    commandEdit_->setMinimumHeight(kFieldHeight);
    form->addRow(tr("Команда:"), commandEdit_);

    root->addWidget(mainGroup);

    /* Environment, Volumes, Ports — мультистрочные */
    auto *envGroup = new QGroupBox(tr("Переменные окружения (по одной KEY=VALUE на строку)"), content);
    {
        auto *l = new QVBoxLayout(envGroup);
        envEdit_ = new QPlainTextEdit(envGroup);
        envEdit_->setPlaceholderText("MYSQL_ROOT_PASSWORD=secret\nDB_NAME=app");
        envEdit_->setFixedHeight(70);
        l->addWidget(envEdit_);
    }
    root->addWidget(envGroup);

    auto *volGroup = new QGroupBox(tr("Монтирования (host:container, по одному на строку)"), content);
    {
        auto *l = new QVBoxLayout(volGroup);
        volumesEdit_ = new QPlainTextEdit(volGroup);
        volumesEdit_->setPlaceholderText("/home/user/app:/app\n/var/data:/data");
        volumesEdit_->setFixedHeight(60);
        l->addWidget(volumesEdit_);
    }
    root->addWidget(volGroup);

    auto *portGroup = new QGroupBox(tr("Проброс портов на физический хост (host:container, по одному на строку)"), content);
    {
        auto *l = new QVBoxLayout(portGroup);
        portsEdit_ = new QPlainTextEdit(portGroup);
        /* В плейсхолдере намеренно не 8080 — он занят Ryu wsapi. */
        portsEdit_->setPlaceholderText("8088:80\n15432:5432");
        portsEdit_->setFixedHeight(60);
        l->addWidget(portsEdit_);
    }
    root->addWidget(portGroup);

    /* Подсказка */
    auto *hint = new QLabel(tr(
        "<i><b>Важно про «Команда»:</b> если оставить пустой — Containernet "
        "запустит контейнер с <code>/bin/bash</code> вместо ENTRYPOINT/CMD "
        "образа, и сервис (postgres/nginx/mysql/…) НЕ стартует. "
        "Пресеты выставляют правильную команду автоматически.<br>"
        "Проброшенные порты доступны на хосте как "
        "<code>http://localhost:&lt;host&gt;</code> — можно открыть в браузере. "
        "Для входа: ПКМ → «Войти в контейнер» "
        "(или <code>sudo docker exec -it mn.%1 bash</code>).<br>"
        "Образы <b>alpine</b> без bash — Containernet повисает на старте, "
        "используйте обычные ubuntu/debian/nginx/postgres/…</i>"
    ).arg(node_->getName()), content);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #94A3B8; padding: 6px;");
    root->addWidget(hint);

    /* OK/Cancel — ВНЕ скролла, прибиты к низу окна. Без обёртки и без
     * stylesheet, иначе тема прячет текст и остаются только иконки. */
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bb->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    bb->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    bb->setContentsMargins(12, 8, 12, 12);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(bb, 0);
}

void DockerDialog::loadFromNode()
{
    nameEdit_->setText(node_->getName());
    int idx = imageBox_->findText(node_->getImage());
    if (idx >= 0) imageBox_->setCurrentIndex(idx);
    else imageBox_->setEditText(node_->getImage());
    ipEdit_->setText(node_->getIp());
    macEdit_->setText(node_->getMac());
    commandEdit_->setText(node_->getCommand());
    envEdit_->setPlainText(node_->getEnvironment().join("\n"));
    volumesEdit_->setPlainText(node_->getVolumes().join("\n"));
    portsEdit_->setPlainText(node_->getPublishedPorts().join("\n"));
}

static QStringList splitLines(const QString &s)
{
    QStringList out;
    for (const QString &line : s.split('\n'))
    {
        QString t = line.trimmed();
        if (!t.isEmpty()) out << t;
    }
    return out;
}

void DockerDialog::accept()
{
    node_->setImage(imageBox_->currentText().trimmed());
    node_->setIp(ipEdit_->text().trimmed());
    node_->setMac(macEdit_->text().trimmed());
    node_->setCommand(commandEdit_->text().trimmed());
    node_->setEnvironment(splitLines(envEdit_->toPlainText()));
    node_->setVolumes(splitLines(volumesEdit_->toPlainText()));
    node_->setPublishedPorts(splitLines(portsEdit_->toPlainText()));
    QDialog::accept();
}

void DockerDialog::applyPreset(int index)
{
    const auto &all = presets();
    if (index <= 0 || index >= all.size())
    {
        presetHint_->setVisible(false);
        presetHint_->clear();
        return;
    }
    const DockerPreset &p = all.at(index);

    int idx = imageBox_->findText(p.image);
    if (idx >= 0) imageBox_->setCurrentIndex(idx);
    else imageBox_->setEditText(p.image);

    commandEdit_->setText(p.command);
    envEdit_->setPlainText(p.env.join("\n"));
    portsEdit_->setPlainText(p.ports.join("\n"));

    const QString ip = ipEdit_->text().trimmed().isEmpty()
                       ? QStringLiteral("10.0.0.X")
                       : ipEdit_->text().trimmed();
    presetHint_->setText(p.testHint.arg(ip, node_->getName()));
    presetHint_->setTextFormat(Qt::RichText);
    presetHint_->setOpenExternalLinks(true);
    presetHint_->setVisible(!p.testHint.isEmpty());
}
