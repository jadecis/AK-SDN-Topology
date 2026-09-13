#ifndef IROUTINGALGORITHM_H
#define IROUTINGALGORITHM_H

#include <QString>
#include <QList>
#include "Graph.h"

/* Базовый интерфейс встроенных алгоритмов маршрутизации.
 *
 * Конвенция: индексы соответствуют позиции коммутатора в QList<Switch *>,
 * получаемом из NetworkMap::getSwitches(). Преобразование индекс ↔ groupId
 * выполняется в Core (groupId = index + 1).
 *
 * computeTree возвращает список рёбер дерева оптимальных маршрутов
 * в формате [[a_idx, b_idx], ...]; пустой список — если корень
 * изолирован или вход некорректен.
 *
 * computePath возвращает упорядоченную последовательность узлов
 * от from до to включительно; пустой список — если путь не найден.
 */
class IRoutingAlgorithm
{
public:
    virtual ~IRoutingAlgorithm() = default;

    virtual QString id() const = 0;            // "dijkstra"
    virtual QString displayName() const = 0;   // "Дейкстра"

    /* Тип визуализации:
     *   "tree"     — алгоритм строит дерево/маршруты (Дейкстра, LARAC)
     *   "segments" — алгоритм разбивает сеть на сегменты (Greedy Segmentation)
     *   "path"     — алгоритм выдаёт один путь (CSP/MCOP) */
    virtual QString kind() const { return QStringLiteral("tree"); }

    /* Полностью пересчитывать маршруты при каждом изменении канала?
     * false — например для парных переходов, где изменение метрики
     * на одном канале не требует пересчёта всего ДОМ. */
    virtual bool needsFullRecompute() const { return true; }

    virtual QList<QList<int>> computeTree(const Graph &graph, int root) = 0;
    virtual QList<int> computePath(const Graph &graph, int from, int to) = 0;

    /* Сегментация: возвращает список сегментов (для алгоритмов с kind="segments").
     * Каждый сегмент — список индексов узлов. По умолчанию: один сегмент со
     * всеми узлами. k — желаемое число сегментов (если применимо). */
    virtual QList<QList<int>> computeSegments(const Graph &graph, int k = 2)
    {
        Q_UNUSED(k);
        QList<int> all;
        for (int i = 0; i < graph.size(); ++i) all << i;
        return { all };
    }
};

#endif // IROUTINGALGORITHM_H
