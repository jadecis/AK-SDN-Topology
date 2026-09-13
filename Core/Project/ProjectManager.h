#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QString>

class NetworkMap;

/* ProjectManager — управляет «проектом» как папкой с набором файлов:
 *
 *   <projectDir>/
 *     ├── topology.sdn.xml   — описание топологии (для редактора)
 *     ├── topology.sdn.py    — сгенерированный Mininet-скрипт
 *     ├── metrics.txt        — экспорт матрицы весов
 *     ├── controller.py      — стартовый Ryu-контроллер (simple_switch_13)
 *     ├── README.md          — краткая инструкция запуска
 *     └── .snet/project.json — метаданные проекта (последнее сохранение и т.п.)
 *
 * Поддерживаются шаблоны: Empty (только c0), Ring (c0+3 свитча+3 хоста кольцом),
 * Tree (c0+корневой свитч+2 ветви по 2 хоста).
 */
class ProjectManager : public QObject
{
    Q_OBJECT
public:
    /* Шаблоны топологий — выводятся в NewProjectDialog в порядке
     * от простого к сложному. Значения стабильны (хранятся в .sdn.xml
     * проектов — менять нельзя без миграции). */
    enum Template {
        Empty = 0,            // пустая, только контроллер
        Ring  = 1,            // [LEGACY] кольцо 3 свитчей
        Tree  = 2,            // [LEGACY] базовое дерево
        Star          = 3,    // 1 свитч + 4 хоста (PDF рис. 1.12)
        LinearFour    = 4,    // 4 свитча в линию + 4 хоста (PDF рис. 1.13)
        RingFive      = 5,    // кольцо 5 свитчей — демонстрация парных переходов
        TreeSeven     = 6,    // дерево 7 свитчей + 8 хостов (PDF рис. 1.14)
        MeshDocker    = 7,    // mesh 6 свитчей + 4 хоста + 2 Docker — для активной маршрутизации
    };

    explicit ProjectManager(QObject *parent = nullptr);

    /* Создаёт новый проект — каталог + начальный набор файлов.
     * map (если не null) очищается и заполняется по выбранному шаблону. */
    bool createProject(const QString &projectDir,
                       const QString &projectName,
                       Template tmpl,
                       NetworkMap *map,
                       QString *errorOut = nullptr);

    /* Открывает проект из существующего каталога. Загружает topology.sdn.xml в map. */
    bool openProject(const QString &projectDir,
                     NetworkMap *map,
                     QString *errorOut = nullptr);

    /* Сохраняет текущее состояние во все файлы проекта (xml/py/metrics). */
    bool saveAll(NetworkMap *map, QString *errorOut = nullptr);

    /* Закрывает проект (без сохранения). */
    void closeProject();

    /* Геттеры */
    QString projectDir() const { return projectDir_; }
    QString projectName() const { return projectName_; }
    bool isOpen() const { return !projectDir_.isEmpty(); }

    /* Пути к ключевым файлам проекта (валидны после open/create). */
    QString topologyXmlPath() const;
    QString mininetScriptPath() const;
    QString metricsPath() const;
    QString controllerPyPath() const;   // legacy, теперь = ryuMainPyPath()
    QString readmePath() const;

    /* Папка с Ryu-приложением (ryu/) и его файлами. */
    QString ryuDir() const;
    QString ryuMainPyPath() const;       // ryu/ryu_app_controller.py
    QString userAlgorithmsDir() const;   // ryu/algorithms_user/

    /* Создать пользовательский алгоритм по шаблону.
     * Возвращает путь к созданному файлу или пустую строку при ошибке. */
    QString createUserAlgorithm(const QString &fileName, const QString &className,
                                const QString &algorithmName, QString *errorOut = nullptr);

    /* Список последних открытых проектов (хранится в QSettings). */
    static QStringList recentProjects();
    static void addRecentProject(const QString &dir);
    static void removeRecentProject(const QString &dir);

signals:
    void projectOpened(QString dir, QString name);
    void projectClosed();
    void projectSaved();

private:
    QString projectDir_;
    QString projectName_;

    void writeText(const QString &path, const QString &content) const;
    bool copyResource(const QString &resourcePath, const QString &destPath,
                      bool overwrite = false) const;
    void deployRyuTemplates(const QString &projectName) const;
    QString defaultReadmeMd(const QString &projectName) const;
    QString defaultProjectMetaJson(const QString &projectName) const;
    void buildTemplate(Template tmpl, NetworkMap *map) const;
};

#endif // PROJECTMANAGER_H
