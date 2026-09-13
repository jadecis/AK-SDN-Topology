#include "AlgorithmRegistry.h"
#include "DijkstraAlgorithm.h"
#include "LARACAlgorithm.h"
#include "GreedySegmentation.h"
#include "PairTransitionAlgorithm.h"

AlgorithmRegistry *AlgorithmRegistry::instance()
{
    static AlgorithmRegistry r;
    return &r;
}

AlgorithmRegistry::AlgorithmRegistry()
{
    /* Порядок регистрации = порядок отображения в UI. Дейкстра — первая
     * (по умолчанию). */
    registerAlgorithm(new DijkstraAlgorithm());
    registerAlgorithm(new PairTransitionAlgorithm());
    registerAlgorithm(new LARACAlgorithm());
    registerAlgorithm(new GreedySegmentation());
}

AlgorithmRegistry::~AlgorithmRegistry()
{
    qDeleteAll(m_order);
}

void AlgorithmRegistry::registerAlgorithm(IRoutingAlgorithm *algo)
{
    m_byId.insert(algo->id(), algo);
    m_order.append(algo);
}

IRoutingAlgorithm *AlgorithmRegistry::get(const QString &id) const
{
    return m_byId.value(id, nullptr);
}

QList<IRoutingAlgorithm *> AlgorithmRegistry::all() const
{
    return m_order;
}

QStringList AlgorithmRegistry::ids() const
{
    QStringList result;
    for (IRoutingAlgorithm *a : m_order)
        result << a->id();
    return result;
}
