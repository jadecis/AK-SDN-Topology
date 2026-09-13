#include "ProcessLauncher.h"
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryFile>

ProcessLauncher::ProcessLauncher(QObject *parent) :
    QObject(parent)
{
}

QString ProcessLauncher::detectTerminal(QStringList &argsOut) const
{
    /* Список приоритета: gnome-terminal -> xfce4-terminal -> konsole -> xterm.
     * Каждый со своими формами вызова "-- bash -c <cmd>". */

    struct Candidate {
        const char *program;
        QStringList args;
    };

    const Candidate candidates[] = {
        { "gnome-terminal",   { "--", "bash", "-c" } },
        { "xfce4-terminal",   { "-x", "bash", "-c" } },
        { "konsole",          { "-e", "bash", "-c" } },
        { "xterm",            { "-e", "bash", "-c" } },
        { "qterminal",        { "-e", "bash", "-c" } },
        { "lxterminal",       { "-e", "bash", "-c" } }
    };

    for (const Candidate &c : candidates)
    {
        QString found = QStandardPaths::findExecutable(c.program);
        if (!found.isEmpty())
        {
            argsOut = c.args;
            return found;
        }
    }

#ifdef Q_OS_WIN
    /* На Windows используем cmd.exe — для нужд разработки/сборки. */
    argsOut = QStringList() << "/C";
    QString cmd = QStandardPaths::findExecutable("cmd.exe");
    if (!cmd.isEmpty())
    {
        return cmd;
    }
#endif

    argsOut.clear();
    return QString();
}

bool ProcessLauncher::launchMininet(const QString &scriptPath,
                                    const QStringList &extraArgs,
                                    QString *errorOut)
{
    QFileInfo info(scriptPath);
    if (!info.exists())
    {
        QString err = tr("Скрипт не найден: %1").arg(scriptPath);
        if (errorOut) *errorOut = err;
        emit failed(err);
        return false;
    }

    QStringList termArgs;
    QString terminal = detectTerminal(termArgs);
    if (terminal.isEmpty())
    {
        QString err = tr("Не удалось найти терминальный эмулятор (gnome-terminal/xterm/konsole)");
        if (errorOut) *errorOut = err;
        emit failed(err);
        return false;
    }

    /* Mininet запускается в терминале через sudo -A + SUDO_ASKPASS,
     * НЕ через `echo PASS | sudo -S` — последний закрывает stdin
     * после передачи пароля, и Mininet CLI получает EOF, мгновенно
     * выходя из «mininet>».
     *
     * SUDO_ASKPASS — путь к скрипту, который sudo дёрнет для получения
     * пароля. Создаём такой скрипт во временном файле с правами 0700,
     * и удаляем сразу после старта Mininet (bash trap EXIT).
     *
     * Скрипт пишет свой PID в /tmp/snet_mininet.pid — для stopTopology. */
    QString askpassPath = createAskpassScript();
    if (askpassPath.isEmpty())
    {
        QString err = tr("Не удалось создать askpass-скрипт для sudo");
        if (errorOut) *errorOut = err;
        emit failed(err);
        return false;
    }

    QString bashCmd = QString(
        "cd '%1' && "
        "echo '=== SNet: Mininet топология ==='; "
        "export SUDO_ASKPASS='%4'; "
        "trap \"rm -f '%4'\" EXIT; "
        "sudo -A bash -c \""
        "    echo \\$\\$ > /tmp/snet_mininet.pid; "
        "    exec python3 '%2' %3"
        "\"; "
        "echo '--- Mininet завершён, нажмите Enter для закрытия ---'; "
        "read"
    ).arg(info.absolutePath())
     .arg(info.absoluteFilePath())
     .arg(extraArgs.join(' '))
     .arg(askpassPath);

    QStringList args = termArgs;
    args << bashCmd;

    bool ok = QProcess::startDetached(terminal, args);
    if (!ok)
    {
        QString err = tr("Не удалось запустить терминал '%1'").arg(terminal);
        if (errorOut) *errorOut = err;
        emit failed(err);
        return false;
    }

    emit launched(tr("Mininet запущен (скрипт %1, флаги %2)")
                  .arg(info.fileName())
                  .arg(extraArgs.join(' ')));
    return true;
}

bool ProcessLauncher::launchRyu(const QString &ryuAppPath, QString *errorOut)
{
    QFileInfo info(ryuAppPath);
    if (!info.exists())
    {
        QString err = tr("Ryu-приложение не найдено: %1").arg(ryuAppPath);
        if (errorOut) *errorOut = err;
        emit failed(err);
        return false;
    }

    QStringList termArgs;
    QString terminal = detectTerminal(termArgs);
    if (terminal.isEmpty())
    {
        QString err = tr("Не удалось найти терминальный эмулятор");
        if (errorOut) *errorOut = err;
        emit failed(err);
        return false;
    }

    /* Запускаем ryu-manager напрямую в foreground терминала.
     * Никакого subshell/tail — они порождали SIGTERM через bash job control.
     * --observe-links нужен для ryu.topology.api.get_link() (построение
     * графа достижимости в REST-эндпоинте /topology). */
    QString bashCmd = QString(
        "set +e; "
        "cd '%1' || exit 1; "
        "echo '=== SNet: Ryu контроллер ==='; "
        "echo 'cwd:' $(pwd); "
        "echo 'python3:' $(python3 --version 2>&1); "
        "echo 'ryu-manager:' $(which ryu-manager 2>&1); "
        "echo; "
        "PYTHONUNBUFFERED=1 PYTHONPATH=. ryu-manager "
        "  --observe-links "
        "  --wsapi-port 8080 --wsapi-host 127.0.0.1 "
        "  '%2'; "
        "RC=$?; "
        "echo; "
        "echo \"=== ryu-manager завершён (exit $RC) ===\"; "
        "echo '--- Нажмите Enter для закрытия ---'; "
        "read")
        .arg(info.absolutePath())
        .arg(info.absoluteFilePath());

    QStringList args = termArgs;
    args << bashCmd;

    bool ok = QProcess::startDetached(terminal, args);
    if (!ok)
    {
        QString err = tr("Не удалось запустить терминал '%1'").arg(terminal);
        if (errorOut) *errorOut = err;
        emit failed(err);
        return false;
    }

    emit launched(tr("Ryu запущен (приложение %1)").arg(info.fileName()));
    return true;
}

QString ProcessLauncher::createAskpassScript() const
{
    if (sudoPassword_.isEmpty())
        return QString();

    /* Создаём файл в TMP с уникальным именем. autoRemove=false — пусть
     * bash сам удалит через trap (тогда даже при crash'е Qt файл не
     * останется). */
    QTemporaryFile tmp(QDir::tempPath() + "/snet_askpass_XXXXXX.sh");
    tmp.setAutoRemove(false);
    if (!tmp.open())
        return QString();

    /* В пароле могут быть одинарные кавычки — экранируем 'A'\''B' стилем. */
    QString escaped = sudoPassword_;
    escaped.replace("'", "'\\''");

    QByteArray body;
    body.append("#!/bin/bash\n");
    body.append("printf '%s\\n' '");
    body.append(escaped.toUtf8());
    body.append("'\n");
    tmp.write(body);
    tmp.close();

    QFile::setPermissions(tmp.fileName(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner
                          | QFileDevice::ExeOwner);
    return tmp.fileName();
}

void ProcessLauncher::writeSudoPassword(QProcess *p) const
{
    if (!p) return;
    p->write((sudoPassword_ + "\n").toUtf8());
    p->closeWriteChannel();
}

QProcess *ProcessLauncher::runSudo(const QStringList &cmdArgs,
                                   const std::function<void(int)> &onFinished)
{
    /* sudo -S читает пароль из stdin. -p '' — пустой prompt чтобы не путать
     * парсеры. После пароля идёт собственно команда. */
    QStringList args;
    args << "-S" << "-p" << "";
    args += cmdArgs;

    QProcess *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [p, onFinished](int code, QProcess::ExitStatus) {
                if (onFinished) onFinished(code);
                p->deleteLater();
            });
    connect(p, &QProcess::errorOccurred, this, [p, onFinished](QProcess::ProcessError) {
        if (onFinished) onFinished(-1);
        p->deleteLater();
    });
    p->start("sudo", args);
    if (!p->waitForStarted(2000))
    {
        if (onFinished) onFinished(-1);
        p->deleteLater();
        return nullptr;
    }
    /* Передаём пароль на stdin и закрываем write-channel. */
    p->write((sudoPassword_ + "\n").toUtf8());
    p->closeWriteChannel();
    return p;
}

void ProcessLauncher::runMininetCleanup(const QString &label)
{
    /* `sudo -S mn -c` в фоне с паролем через stdin. Никаких терминалов —
     * fully асинхронно, событие mininetCleaned приходит когда mn -c всё
     * завершит. mn -c также делает `killall ryu-manager`, поэтому
     * runMininetCleanup() надо вызывать ДО launchRyu(). */
    if (sudoPassword_.isEmpty())
    {
        emit failed(tr("Пароль sudo не введён. Откройте топологию заново."));
        emit mininetCleaned();
        return;
    }
    QString tag = label.isEmpty() ? QStringLiteral("перед запуском") : label;
    emit launched(tr("Очистка Mininet (%1)…").arg(tag));

    QProcess *p = runSudo(QStringList() << "mn" << "-c",
                          [this, tag](int code) {
        if (code == 0)
            emit launched(tr("Очистка Mininet завершена (%1)").arg(tag));
        else
            emit failed(tr("Очистка Mininet (%1): код %2").arg(tag).arg(code));
        emit mininetCleaned();
    });
    if (!p)
    {
        emit failed(tr("Не удалось запустить sudo для mn -c"));
        emit mininetCleaned();
    }
}

void ProcessLauncher::killRyu()
{
    /* ryu-manager у нас от обычного пользователя — sudo не нужен. */
    QProcess::startDetached("pkill", QStringList() << "-f" << "ryu-manager");
}

void ProcessLauncher::killMininetScript()
{
    /* Mininet работает под sudo, без sudo pkill не убьёт.
     *
     * Делаем АГРЕССИВНУЮ очистку — убиваем ВСЕХ топология.sdn.py и
     * mininet процессов от user'а stu. Это нужно потому что прошлые
     * сессии могли оставить fallback-pingall-каскады (известный баг
     * итерации 10) которые держат OVS-мосты и блокируют новую топологию.
     *
     * Сначала PID из /tmp/snet_mininet.pid (текущая сессия), потом
     * глобальный pkill всех *.sdn.py чтобы убрать зомби. */
    if (sudoPassword_.isEmpty())
    {
        return;
    }
    QStringList cmd;
    cmd << "bash" << "-c"
        << "PID=$(cat /tmp/snet_mininet.pid 2>/dev/null); "
           "if [ -n \"$PID\" ]; then "
           "  pkill -TERM -P $PID 2>/dev/null; "
           "  kill -TERM $PID 2>/dev/null; "
           "fi; "
           "# Агрессивная очистка зомби от прошлых сессий: "
           "pkill -9 -f 'topology.sdn.py' 2>/dev/null; "
           "pkill -9 -f 'snet_topology.sdn.py' 2>/dev/null; "
           "rm -f /tmp/snet_mininet.pid; "
           "true";
    runSudo(cmd, nullptr);
}

QString ProcessLauncher::runCommandCaptured(const QString &program,
                                            const QStringList &args,
                                            int timeoutMs,
                                            int *exitCodeOut)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);
    if (!proc.waitForStarted(timeoutMs))
    {
        if (exitCodeOut) *exitCodeOut = -1;
        return QString();
    }
    if (!proc.waitForFinished(timeoutMs))
    {
        proc.kill();
        if (exitCodeOut) *exitCodeOut = -1;
        return QString::fromLocal8Bit(proc.readAllStandardOutput());
    }
    if (exitCodeOut) *exitCodeOut = proc.exitCode();
    return QString::fromLocal8Bit(proc.readAllStandardOutput());
}
