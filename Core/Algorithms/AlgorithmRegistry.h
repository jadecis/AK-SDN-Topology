#ifndef ALGORITHMREGISTRY_H
#define ALGORITHMREGISTRY_H

#include "IRoutingAlgorithm.h"
#include <QHash>
#include <QString>
#include <QList>

/* AlgorithmRegistry — singleton-реестр всех встроенных алгоритмов
 * маршрутизации. Используется UI'ем (AlgorithmDialog) для генерации
 * списка вариантов и Core'ом для запуска. */
class AlgorithmRegistry
{
public:
    static AlgorithmRegistry *instance();

    IRoutingAlgorithm *get(const QString &id) const;
    QList<IRoutingAlgorithm *> all() const;
    QStringList ids() const;

private:
    AlgorithmRegistry();
    ~AlgorithmRegistry();

    void registerAlgorithm(IRoutingAlgorithm *algo);

    QHash<QString, IRoutingAlgorithm *> m_byId;
    QList<IRoutingAlgorithm *> m_order;
};

#endif // ALGORITHMREGISTRY_H
