#ifndef DOCKERNODE_H
#define DOCKERNODE_H

#include "Node.h"
#include <QStringList>

/* DockerNode — узел-Docker-контейнер.
 *
 * В Mininet генерируется через Containernet:
 *     d1 = net.addDocker('d1', ip='10.0.0.10', dimage='nginx:alpine')
 *
 * Подключается к свитчу обычным линком (как Host).
 * Полезно для развертывания реальных приложений (БД, веб-серверы) в
 * SDN-топологии и тестирования с обычных Mininet-хостов.
 */
class DockerNode : public Node
{
public:
    /* Состояние контейнера в рантайме (по данным /docker/health). Не
     * сохраняется в проект — обновляется поллингом во время работы Mininet. */
    enum RuntimeStatus { StatusUnknown = 0, StatusHealthy, StatusStarting, StatusDead };

    DockerNode(QPoint position);
    ~DockerNode();

    inline RuntimeStatus getRuntimeStatus() const { return runtimeStatus_; }
    inline void setRuntimeStatus(RuntimeStatus s) { runtimeStatus_ = s; }

    inline QString getMac() const { return mac_; }
    inline void setMac(QString mac) { mac_ = mac; }
    inline QString getIp() const { return ip_; }
    inline void setIp(QString ip) { ip_ = ip; }

    /* Docker-специфичные поля. */
    inline QString getImage() const { return image_; }
    inline void setImage(QString image) { image_ = image; }
    /* Команда, выполняемая в контейнере вместо CMD из образа.
     * Пустая строка — использовать CMD/ENTRYPOINT из image. */
    inline QString getCommand() const { return command_; }
    inline void setCommand(QString cmd) { command_ = cmd; }
    /* Переменные окружения в формате "KEY=VALUE", по одной на строку. */
    inline QStringList getEnvironment() const { return env_; }
    inline void setEnvironment(QStringList env) { env_ = env; }
    /* Монтирования host:container, по одному на строку. */
    inline QStringList getVolumes() const { return volumes_; }
    inline void setVolumes(QStringList v) { volumes_ = v; }
    /* Опубликованные порты host:container, по одному на строку. */
    inline QStringList getPublishedPorts() const { return publishedPorts_; }
    inline void setPublishedPorts(QStringList p) { publishedPorts_ = p; }

    virtual void configure();
    virtual void setup(int num);

    virtual void draw(NetworkMapDrawer *drawer);
    virtual DeviceType getDeviceType();
    virtual QSize getSize() const;

private:
    QString mac_;
    QString ip_;
    QString image_;     // например "nginx:alpine", "mysql:8", "python:3.11-slim"
    QString command_;   // CMD override
    QStringList env_;
    QStringList volumes_;
    QStringList publishedPorts_;
    RuntimeStatus runtimeStatus_ = StatusUnknown;
};

#endif // DOCKERNODE_H
