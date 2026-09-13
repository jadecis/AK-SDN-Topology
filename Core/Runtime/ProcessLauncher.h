#ifndef PROCESSLAUNCHER_H
#define PROCESSLAUNCHER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QProcess;

/* ProcessLauncher — отвечает за запуск внешних процессов из приложения:
 *   • Mininet-скрипта в отдельном терминале;
 *   • Ryu-приложения в отдельном терминале;
 *   • любой произвольной shell-команды (служебные нужды, парсинг pingall).
 *
 * Реализация ориентирована на Ubuntu; обнаруживает поддерживаемые терминалы
 * (gnome-terminal, xfce4-terminal, konsole, xterm) и использует первый из них.
 * На Windows fallback: cmd.exe — для подкачки разработки.
 */
class ProcessLauncher : public QObject
{
    Q_OBJECT
public:
    explicit ProcessLauncher(QObject *parent = nullptr);

    /* Запустить Mininet-скрипт в новом терминальном окне.
     * extraArgs передаются скрипту (например, "--http 5555"). */
    bool launchMininet(const QString &scriptPath,
                       const QStringList &extraArgs = QStringList(),
                       QString *errorOut = nullptr);

    /* Запустить Ryu-приложение. */
    bool launchRyu(const QString &ryuAppPath,
                   QString *errorOut = nullptr);

    /* Сохранить пароль sudo в памяти процесса. Используется в sudo -S. */
    void setSudoPassword(const QString &password) { sudoPassword_ = password; }
    bool hasSudoPassword() const { return !sudoPassword_.isEmpty(); }
    void clearSudoPassword() { sudoPassword_.clear(); }

    /* Записать сохранённый пароль в stdin уже запущенного QProcess
     * (sudo -S получит его) и закрыть write channel. */
    void writeSudoPassword(QProcess *p) const;

    /* Запустить `sudo -S mn -c` в фоне (без терминала). Передаёт пароль
     * через stdin. Когда процесс завершится — испускается mininetCleaned. */
    void runMininetCleanup(const QString &label = QString());

    /* Принудительно завершить ryu-manager (pkill -f ryu-manager).
     * Без sudo, поскольку ryu запущен от обычного пользователя. */
    void killRyu();

    /* Убить Mininet Python-процесс через sudo -S pkill. */
    void killMininetScript();

    /* Запустить произвольную bash-команду в скрытом фоновом QProcess'е
     * с захватом stdout. */
    static QString runCommandCaptured(const QString &program,
                                      const QStringList &args,
                                      int timeoutMs = 5000,
                                      int *exitCodeOut = nullptr);

    /* Создаёт временный исполняемый файл с правами 0700, который при
     * запуске печатает текущий sudoPassword_ на stdout. Используется как
     * SUDO_ASKPASS — позволяет sudo получить пароль БЕЗ закрытия stdin
     * дочернего процесса (нужно для Mininet CLI и docker exec).
     * Bash-скрипт сам удалит файл через trap EXIT. */
    QString createAskpassScript() const;

signals:
    void launched(const QString &message);
    void failed(const QString &reason);
    /* Испускается когда `sudo mn -c` (запущенный через runMininetCleanup) завершился. */
    void mininetCleaned();

private:
    /* Возвращает путь к найденному терминальному эмулятору и список аргументов
     * вида ["--", "bash", "-lc"] (или аналогичный). Если ничего не найдено —
     * возвращает пустую строку. */
    QString detectTerminal(QStringList &argsOut) const;

    /* Запустить sudo -S <args> в фоне; пароль шлётся через stdin.
     * Возвращает QProcess (newly allocated, deleteLater при finished).
     * onFinished — необязательный callback (int exitCode). */
    QProcess *runSudo(const QStringList &cmdArgs,
                      const std::function<void(int)> &onFinished = nullptr);

    QString sudoPassword_;
};

#endif // PROCESSLAUNCHER_H
