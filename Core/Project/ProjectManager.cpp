#include "ProjectManager.h"
#include "NetworkMap.h"
#include "XmlDeserializer.h"
#include "IO.h"
#include "WeightsMatrix.h"
#include "SdnController.h"
#include "Switch.h"
#include "Host.h"
#include "DockerNode.h"
#include "SSLink.h"

#define _USE_MATH_DEFINES
#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>

ProjectManager::ProjectManager(QObject *parent) : QObject(parent)
{
}

QString ProjectManager::topologyXmlPath() const
{
    return QDir(projectDir_).filePath("topology.sdn.xml");
}
QString ProjectManager::mininetScriptPath() const
{
    return QDir(projectDir_).filePath("topology.sdn.py");
}
QString ProjectManager::metricsPath() const
{
    return QDir(projectDir_).filePath("metrics.txt");
}
QString ProjectManager::controllerPyPath() const
{
    /* Главный Ryu-app: ryu/ryu_app_controller.py */
    return ryuMainPyPath();
}
QString ProjectManager::readmePath() const
{
    return QDir(projectDir_).filePath("README.md");
}
QString ProjectManager::ryuDir() const
{
    return QDir(projectDir_).filePath("ryu");
}
QString ProjectManager::ryuMainPyPath() const
{
    return QDir(ryuDir()).filePath("ryu_app_controller.py");
}
QString ProjectManager::userAlgorithmsDir() const
{
    return QDir(ryuDir()).filePath("algorithms_user");
}

bool ProjectManager::copyResource(const QString &resourcePath,
                                  const QString &destPath,
                                  bool overwrite) const
{
    QFile dst(destPath);
    if (dst.exists() && !overwrite) return true;
    if (dst.exists() && overwrite)
    {
        dst.remove();
    }
    QFile src(resourcePath);
    if (!src.open(QIODevice::ReadOnly))
    {
        return false;
    }
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        src.close();
        return false;
    }
    dst.write(src.readAll());
    src.close();
    dst.close();
    /* Делаем .py исполняемым (только на Unix) */
    dst.setPermissions(dst.permissions()
                       | QFileDevice::ExeOwner
                       | QFileDevice::ReadOwner
                       | QFileDevice::WriteOwner);
    return true;
}

void ProjectManager::deployRyuTemplates(const QString &projectName) const
{
    Q_UNUSED(projectName);
    QString ryu = ryuDir();
    QString userDir = userAlgorithmsDir();
    QDir().mkpath(ryu);
    QDir().mkpath(userDir);

    struct File { const char *res; const char *rel; };
    const File files[] = {
        { ":/templates/templates/ryu/ryu_app_controller.py",   "ryu_app_controller.py" },
        { ":/templates/templates/ryu/ryu_app_rest_routing.py", "ryu_app_rest_routing.py" },
        { ":/templates/templates/ryu/algorithms.py",           "algorithms.py" },
        { ":/templates/templates/ryu/base_algorithm.py",       "base_algorithm.py" },
        { ":/templates/templates/ryu/sdn_topology.py",         "sdn_topology.py" },
    };
    for (const File &f : files)
    {
        copyResource(f.res, QDir(ryu).filePath(f.rel),
                     /* overwrite= */ true);
    }

    /* Пример пользовательского алгоритма — копируем только если папка
     * пуста (чтобы не затирать пользовательский код). */
    QDir userQDir(userDir);
    if (userQDir.entryList(QStringList() << "*.py",
                           QDir::Files).isEmpty())
    {
        copyResource(":/templates/templates/ryu/algorithms_user/example_my_algorithm.py",
                     userQDir.filePath("example_my_algorithm.py"));
    }
    /* __init__.py — пустой, нужен для импорта */
    QString initPath = userQDir.filePath("__init__.py");
    if (!QFileInfo::exists(initPath))
    {
        writeText(initPath, "# SNet user algorithms package\n");
    }
}

QString ProjectManager::createUserAlgorithm(const QString &fileName,
                                            const QString &className,
                                            const QString &algorithmName,
                                            QString *errorOut)
{
    if (!isOpen())
    {
        if (errorOut) *errorOut = tr("Проект не открыт");
        return QString();
    }
    QString safeFile = fileName.endsWith(".py") ? fileName : fileName + ".py";
    QString fullPath = QDir(userAlgorithmsDir()).filePath(safeFile);
    if (QFileInfo::exists(fullPath))
    {
        if (errorOut) *errorOut = tr("Файл уже существует: %1").arg(fullPath);
        return QString();
    }
    QString tpl = QString(
        "\"\"\"\n"
        "Пользовательский алгоритм маршрутизации.\n"
        "Создано через SDN Topology (SNet).\n"
        "\"\"\"\n"
        "\n"
        "from base_algorithm import BaseAlgorithm\n"
        "\n"
        "\n"
        "class %1(BaseAlgorithm):\n"
        "    name = \"%2\"\n"
        "    description = \"\"\n"
        "    params = {\n"
        "        \"metric\": (\"str\", \"delay\", \"Метрика веса\"),\n"
        "    }\n"
        "\n"
        "    def run(self, src, dst, graphs, params):\n"
        "        metric = params.get(\"metric\", \"delay\")\n"
        "        graph = graphs.get(metric, {})\n"
        "        self.log(\"Запуск алгоритма '%2'\", \"от\", src, \"до\", dst)\n"
        "        # TODO: ваш алгоритм здесь.\n"
        "        # Должен вернуть dict с полями routes / tree / segments / cost.\n"
        "        return {\n"
        "            \"routes\": [],\n"
        "            \"cost\": 0,\n"
        "        }\n"
    ).arg(className).arg(algorithmName);
    writeText(fullPath, tpl);
    return fullPath;
}

void ProjectManager::writeText(const QString &path, const QString &content) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out.setCodec("UTF-8");
    out << content;
}

/* === Хелперы для шаблонов === */

namespace {

inline SSLink *mkLink(NetworkMap *map, Node *a, Node *b,
                      float delay = 5, float bw = 100, float loss = 0)
{
    SSLink *l = new SSLink(a, b);
    l->setDelay(delay);
    l->setBandwidth(bw);
    l->setPacketLossRate(loss);
    map->addSSLink(l);
    return l;
}

inline Switch *mkSwitch(NetworkMap *map, int x, int y)
{
    Switch *s = new Switch(QPoint(x, y));
    map->addSwitch(s);
    return s;
}

inline Host *mkHost(NetworkMap *map, int x, int y)
{
    Host *h = new Host(QPoint(x, y));
    map->addHost(h);
    return h;
}

inline DockerNode *mkDocker(NetworkMap *map, int x, int y,
                            const QString &image,
                            const QString &dcmd = QString(),
                            const QStringList &env = {},
                            const QStringList &ports = {})
{
    DockerNode *d = new DockerNode(QPoint(x, y));
    d->setImage(image);
    /* dcmd обязателен для сервисных образов — Containernet иначе
     * подменит CMD на /bin/bash и сервис не стартует.
     * Подробнее: memory/project_containernet_dcmd.md */
    if (!dcmd.isEmpty()) d->setCommand(dcmd);
    if (!env.isEmpty())  d->setEnvironment(env);
    if (!ports.isEmpty()) d->setPublishedPorts(ports);
    map->addDockerNode(d);
    return d;
}

} // namespace

void ProjectManager::buildTemplate(Template tmpl, NetworkMap *map) const
{
    if (!map) return;
    map->clear();

    /* Контроллер всегда вверху-слева — нужен сразу для любой топологии. */
    SdnController *c0 = new SdnController(QPoint(80, 80));
    map->addSdnController(c0);

    switch (tmpl)
    {
    case Empty:
        return;

    /* === Звезда === простейшая: 1 свитч в центре, 4 хоста по сторонам.
     * Подходит для проверки L2-обучения и базовых ping. PDF рис. 1.12. */
    case Star:
    {
        Switch *s1 = mkSwitch(map, 500, 320);
        Host *h1 = mkHost(map, 300, 200);
        Host *h2 = mkHost(map, 700, 200);
        Host *h3 = mkHost(map, 300, 460);
        Host *h4 = mkHost(map, 700, 460);
        mkLink(map, h1, s1, 1);
        mkLink(map, h2, s1, 1);
        mkLink(map, h3, s1, 1);
        mkLink(map, h4, s1, 1);
        return;
    }

    /* === Линейная === 4 свитча в цепочке, h1 на s1 и h4 на s4 + два
     * боковых хоста по центру. Идеально для Дейкстры — путь однозначный,
     * легко наблюдать как алгоритм выбирает кратчайший. PDF рис. 1.13. */
    case LinearFour:
    {
        Switch *s1 = mkSwitch(map, 240, 320);
        Switch *s2 = mkSwitch(map, 440, 320);
        Switch *s3 = mkSwitch(map, 640, 320);
        Switch *s4 = mkSwitch(map, 840, 320);
        Host *h1 = mkHost(map, 100, 320);
        Host *h2 = mkHost(map, 440, 480);
        Host *h3 = mkHost(map, 640, 480);
        Host *h4 = mkHost(map, 980, 320);
        mkLink(map, h1, s1, 1);
        mkLink(map, h2, s2, 1);
        mkLink(map, h3, s3, 1);
        mkLink(map, h4, s4, 1);
        mkLink(map, s1, s2, 5);
        mkLink(map, s2, s3, 5);
        mkLink(map, s3, s4, 5);
        return;
    }

    /* === Кольцо-5 === замкнутое кольцо из 5 свитчей, на каждом —
     * по хосту. Между парой свитчей есть два альтернативных маршрута
     * (по и против часовой) — отличный сценарий для **алгоритма парных
     * переходов**: меняешь метрику одного канала кольца → видишь, как
     * Ryu мгновенно переключает трафик на резервный канал. */
    case RingFive:
    {
        const double cx = 500, cy = 320, r = 200;
        Switch *sw[5];
        Host   *hs[5];
        for (int i = 0; i < 5; ++i)
        {
            double angle = -M_PI / 2.0 + 2.0 * M_PI * i / 5.0;
            int sx = int(cx + r * std::cos(angle));
            int sy = int(cy + r * std::sin(angle));
            int hx = int(cx + (r + 110) * std::cos(angle));
            int hy = int(cy + (r + 110) * std::sin(angle));
            sw[i] = mkSwitch(map, sx, sy);
            hs[i] = mkHost(map, hx, hy);
            mkLink(map, hs[i], sw[i], 1);
        }
        /* Разнообразные задержки в кольце — алгоритм должен выбирать
         * не короткий, а оптимальный путь. */
        const float delays[5] = { 5, 10, 7, 12, 6 };
        for (int i = 0; i < 5; ++i)
            mkLink(map, sw[i], sw[(i + 1) % 5], delays[i]);
        return;
    }

    /* === Дерево-7 === бинарное дерево с глубиной 3 (7 свитчей + 8 хостов).
     * PDF рис. 1.14. Подходит для **сегментирования** (явные «острова»
     * в виде поддеревьев), для Дейкстры и сравнительного анализа. */
    case TreeSeven:
    {
        Switch *s1 = mkSwitch(map, 540, 120);          // корень
        Switch *s2 = mkSwitch(map, 340, 250);          // L1 левый
        Switch *s5 = mkSwitch(map, 740, 250);          // L1 правый
        Switch *s3 = mkSwitch(map, 200, 400);          // L2
        Switch *s4 = mkSwitch(map, 420, 400);          // L2
        Switch *s6 = mkSwitch(map, 660, 400);          // L2
        Switch *s7 = mkSwitch(map, 880, 400);          // L2

        Host *h[8];
        const int hx[8] = { 130, 250, 360, 470, 600, 720, 830, 950 };
        const int hy = 540;
        for (int i = 0; i < 8; ++i) h[i] = mkHost(map, hx[i], hy);

        mkLink(map, s1, s2, 5);
        mkLink(map, s1, s5, 5);
        mkLink(map, s2, s3, 3);
        mkLink(map, s2, s4, 3);
        mkLink(map, s5, s6, 3);
        mkLink(map, s5, s7, 3);

        mkLink(map, h[0], s3, 1);
        mkLink(map, h[1], s3, 1);
        mkLink(map, h[2], s4, 1);
        mkLink(map, h[3], s4, 1);
        mkLink(map, h[4], s6, 1);
        mkLink(map, h[5], s6, 1);
        mkLink(map, h[6], s7, 1);
        mkLink(map, h[7], s7, 1);
        return;
    }

    /* === Mesh-6 + Docker === крупная сеть для демо активной маршрутизации:
     * 6 свитчей в кольце с диагоналями (несколько кратчайших путей),
     * 4 хоста-клиента и 2 Docker-узла (БД + web). Идеально, чтобы
     * перебирать алгоритмы и видеть, как меняется реальный трафик h↔d. */
    case MeshDocker:
    {
        /* 6 свитчей по окружности. */
        const double cx = 540, cy = 340, r = 190;
        Switch *sw[6];
        for (int i = 0; i < 6; ++i)
        {
            double angle = -M_PI / 2.0 + 2.0 * M_PI * i / 6.0;
            sw[i] = mkSwitch(map, int(cx + r * std::cos(angle)),
                                  int(cy + r * std::sin(angle)));
        }
        /* Кольцо + 2 диагонали (s0-s3 и s1-s4) — даёт несколько
         * альтернативных путей, на которых интересно сравнивать
         * алгоритмы. */
        const float ringDelays[6] = { 5, 8, 6, 10, 7, 9 };
        for (int i = 0; i < 6; ++i)
            mkLink(map, sw[i], sw[(i + 1) % 6], ringDelays[i]);
        mkLink(map, sw[0], sw[3], 14);
        mkLink(map, sw[1], sw[4], 14);

        /* 4 хоста-клиента по углам. */
        Host *h1 = mkHost(map, int(cx - r - 130), int(cy - 110));
        Host *h2 = mkHost(map, int(cx + r + 130), int(cy - 110));
        Host *h3 = mkHost(map, int(cx - r - 130), int(cy + 110));
        Host *h4 = mkHost(map, int(cx + r + 130), int(cy + 110));
        mkLink(map, h1, sw[5], 1);
        mkLink(map, h2, sw[1], 1);
        mkLink(map, h3, sw[4], 1);
        mkLink(map, h4, sw[2], 1);

        /* 2 Docker-узла с уже выставленными dcmd, env и проброшенными
         * портами — пользователь сразу получает рабочую БД и веб без
         * настройки пресетов вручную.
         *   • db (postgres:16) на sw[3]: localhost:15432, demo/secret/demo
         *   • web (nginx)      на sw[0]: localhost:8088
         * Порты выбраны вне 8080/5555/6633/6653 — занятых SNet/Ryu. */
        DockerNode *db  = mkDocker(
            map, int(cx), int(cy + r + 130), "postgres:16",
            "docker-entrypoint.sh postgres",
            QStringList{"POSTGRES_PASSWORD=secret",
                        "POSTGRES_DB=demo",
                        "POSTGRES_USER=demo"},
            QStringList{"15432:5432"});
        DockerNode *web = mkDocker(
            map, int(cx), int(cy - r - 130), "nginx",
            "/docker-entrypoint.sh nginx -g 'daemon off;'",
            QStringList{},
            QStringList{"8088:80"});
        mkLink(map, db,  sw[3], 1);
        mkLink(map, web, sw[0], 1);
        return;
    }

    /* === LEGACY: старые шаблоны Ring и Tree оставлены ради обратной
     * совместимости с уже сохранёнными проектами. Новые проекты должны
     * использовать RingFive / TreeSeven. === */
    case Ring:
    {
        QPoint cs[3] = { QPoint(450, 200), QPoint(700, 400), QPoint(200, 400) };
        QPoint hpos[3] = { QPoint(450, 80), QPoint(880, 400), QPoint(20, 400) };
        Switch *sws[3];
        Host *hosts[3];
        for (int i = 0; i < 3; ++i)
        {
            sws[i] = mkSwitch(map, cs[i].x(), cs[i].y());
            hosts[i] = mkHost(map, hpos[i].x(), hpos[i].y());
            mkLink(map, hosts[i], sws[i], 5);
        }
        for (int i = 0; i < 3; ++i)
            mkLink(map, sws[i], sws[(i + 1) % 3], 10);
        return;
    }

    case Tree:
    {
        Switch *root = mkSwitch(map, 500, 200);
        Switch *l1 = mkSwitch(map, 300, 400);
        Switch *l2 = mkSwitch(map, 700, 400);
        Host *h1 = mkHost(map, 180, 520);
        Host *h2 = mkHost(map, 380, 520);
        Host *h3 = mkHost(map, 620, 520);
        Host *h4 = mkHost(map, 820, 520);
        mkLink(map, root, l1, 5);
        mkLink(map, root, l2, 5);
        mkLink(map, l1, h1, 2);
        mkLink(map, l1, h2, 2);
        mkLink(map, l2, h3, 2);
        mkLink(map, l2, h4, 2);
        return;
    }
    }
}

bool ProjectManager::createProject(const QString &projectDir,
                                   const QString &projectName,
                                   Template tmpl,
                                   NetworkMap *map,
                                   QString *errorOut)
{
    QDir dir;
    if (!dir.mkpath(projectDir))
    {
        if (errorOut) *errorOut = tr("Не удалось создать каталог: %1").arg(projectDir);
        return false;
    }

    projectDir_ = projectDir;
    projectName_ = projectName;

    /* 1. Построить топологию по шаблону */
    buildTemplate(tmpl, map);

    /* 2. Сохранить файлы */
    QString err;
    if (!saveAll(map, &err))
    {
        if (errorOut) *errorOut = err;
        return false;
    }

    /* 3. Полный набор шаблонов Ryu в подпапке ryu/ */
    deployRyuTemplates(projectName_);

    if (!QFileInfo::exists(readmePath()))
    {
        writeText(readmePath(), defaultReadmeMd(projectName_));
    }
    QDir(projectDir_).mkpath(".snet");
    writeText(QDir(projectDir_).filePath(".snet/project.json"),
              defaultProjectMetaJson(projectName_));

    addRecentProject(projectDir_);
    emit projectOpened(projectDir_, projectName_);
    emit projectSaved();
    return true;
}

bool ProjectManager::openProject(const QString &projectDir, NetworkMap *map, QString *errorOut)
{
    if (!QFileInfo(projectDir).isDir())
    {
        if (errorOut) *errorOut = tr("Папка проекта не существует: %1").arg(projectDir);
        return false;
    }
    projectDir_ = projectDir;
    projectName_ = QFileInfo(projectDir).fileName();

    QString xml = topologyXmlPath();
    if (!QFileInfo::exists(xml))
    {
        if (errorOut) *errorOut = tr("В каталоге проекта нет topology.sdn.xml: %1").arg(xml);
        return false;
    }
    IO io;
    XmlDeserializer deserializer;
    QString xmlDocument = io.readFile(xml);
    if (map)
    {
        map->clear();
        deserializer.deserialize(xmlDocument, map);
    }

    /* Авто-обновление системных файлов Ryu (контроллер + REST + algorithms.py
     * + sdn_topology.py + base_algorithm.py) при каждом открытии проекта.
     * Это даёт пользователю свежий контроллер автоматически после обновления
     * SNet — без ручной правки в ~/sdn-topology-N/ryu/.
     * Пользовательские алгоритмы (algorithms_user/*.py) НЕ трогаем. */
    deployRyuTemplates(projectName_);

    /* Очистить __pycache__ — иначе Python подхватит старый байткод. */
    QString cacheDir = QDir(ryuDir()).filePath("__pycache__");
    if (QFileInfo(cacheDir).isDir())
    {
        QDir(cacheDir).removeRecursively();
    }

    addRecentProject(projectDir_);
    emit projectOpened(projectDir_, projectName_);
    return true;
}

bool ProjectManager::saveAll(NetworkMap *map, QString *errorOut)
{
    if (projectDir_.isEmpty())
    {
        if (errorOut) *errorOut = tr("Проект не открыт");
        return false;
    }
    if (!map)
    {
        if (errorOut) *errorOut = tr("Нет данных для сохранения");
        return false;
    }
    IO io;
    io.writeFile(map->createXmlDocument(), topologyXmlPath());
    io.writeFile(map->createMininetScript(), mininetScriptPath());

    WeightsMatrix wm;
    QString metrics = wm.build(map->getGraphMatrix(), map->getSwitches());
    io.writeFile(metrics, metricsPath());

    emit projectSaved();
    return true;
}

void ProjectManager::closeProject()
{
    projectDir_.clear();
    projectName_.clear();
    emit projectClosed();
}

QString ProjectManager::defaultReadmeMd(const QString &projectName) const
{
    return QString(
        "# %1\n\n"
        "Проект SDN-топологии, созданный в **SDN Topology** (SNet).\n\n"
        "## Файлы\n\n"
        "- `topology.sdn.xml` — описание топологии для редактора.\n"
        "- `topology.sdn.py` — Mininet-скрипт (перегенерируется при сохранении).\n"
        "- `metrics.txt`     — матрица весов каналов (delay / bandwidth / loss).\n"
        "- `ryu/` — Ryu-приложение и алгоритмы маршрутизации:\n"
        "  - `ryu_app_controller.py`   — главный контроллер (запускайте этот файл).\n"
        "  - `ryu_app_rest_routing.py` — REST API + plugin loader.\n"
        "  - `algorithms.py`           — встроенные алгоритмы (Dijkstra, MCP, CSP, LARAC и др.).\n"
        "  - `base_algorithm.py`       — базовый класс для своих алгоритмов.\n"
        "  - `sdn_topology.py`         — TCP-обёртка для визуализации в редакторе.\n"
        "  - `algorithms_user/`        — ваши пользовательские алгоритмы.\n\n"
        "## Запуск\n\n"
        "1. **Запустить топологию** (F5) — откроются 2 терминала с Mininet и Ryu.\n"
        "2. **Открыть веб-интерфейс контроллера** (Ctrl+W) — http://localhost:8080.\n"
        "   Там можно выбрать алгоритм, задать параметры, увидеть лог.\n"
        "3. **Алгоритмы → Создать пользовательский алгоритм…** —\n"
        "   создаст файл-шаблон в `ryu/algorithms_user/`. После редактирования "
        "   нажмите «Перезагрузить алгоритмы» в веб-интерфейсе или просто перезапустите Ryu.\n\n"
        "## Зависимости (Ubuntu)\n\n"
        "```bash\n"
        "sudo apt install mininet xterm\n"
        "pip3 install ryu\n"
        "```\n\n"
        "## Docker-контейнеры (тема диплома)\n\n"
        "Если в топологии добавлены **Docker-узлы**, нужен Containernet:\n\n"
        "```bash\n"
        "sudo apt install docker.io\n"
        "sudo systemctl enable --now docker\n"
        "sudo usermod -aG docker $USER   # перелогиньтесь после этого\n"
        "sudo pip3 install -U git+https://github.com/containernet/containernet.git\n"
        "```\n\n"
        "Полезно (предзагрузить образы, чтобы первый старт не тянул их по сети):\n"
        "```bash\n"
        "docker pull alpine:latest nginx:alpine mysql:8 redis:alpine\n"
        "```\n\n"
        "В свойствах Docker-узла указываете:\n"
        "- **Образ** — например `alpine:latest` (для своего приложения),\n"
        "  `nginx:alpine` (HTTP-сервер), `mysql:8` (БД).\n"
        "- **Команда** — переопределение CMD; пусто = использовать из образа.\n"
        "- **Environment** — переменные окружения (например `MYSQL_ROOT_PASSWORD=secret`).\n"
        "- **Volumes** — монтирования host:container для своего кода/данных.\n"
        "- **Порты** — проброс host:container для доступа извне Mininet.\n\n"
        "После запуска топологии можно войти в контейнер:\n"
        "```bash\n"
        "docker exec -it mn.d1 sh\n"
        "```\n"
        "(или ПКМ по Docker-узлу → «Войти в контейнер»).\n"
        ).arg(projectName);
}

QString ProjectManager::defaultProjectMetaJson(const QString &projectName) const
{
    return QString(
        "{\n"
        "    \"name\": \"%1\",\n"
        "    \"version\": 1,\n"
        "    \"editor\": \"SDN Topology (SNet)\"\n"
        "}\n"
        ).arg(projectName);
}

QStringList ProjectManager::recentProjects()
{
    QSettings s("SNet", "Editor");
    return s.value("project/recent").toStringList();
}

void ProjectManager::addRecentProject(const QString &dir)
{
    QSettings s("SNet", "Editor");
    QStringList list = s.value("project/recent").toStringList();
    list.removeAll(dir);
    list.prepend(dir);
    while (list.size() > 8) list.removeLast();
    s.setValue("project/recent", list);
    s.setValue("project/last_open", dir);
}

void ProjectManager::removeRecentProject(const QString &dir)
{
    QSettings s("SNet", "Editor");
    QStringList list = s.value("project/recent").toStringList();
    list.removeAll(dir);
    s.setValue("project/recent", list);
}
