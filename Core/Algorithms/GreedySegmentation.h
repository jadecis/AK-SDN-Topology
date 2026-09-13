#ifndef GREEDYSEGMENTATION_H
#define GREEDYSEGMENTATION_H

#include "IRoutingAlgorithm.h"

/* Жадное сегментирование (§7.3.2 пособия): разбиение сети на k сегментов
 * («островов»). В демонстрационной версии k=2 — два сегмента по принципу
 * минимального пути от root.
 *
 * computeTree формирует дерево минимальных расстояний из root, аналогично
 * Дейкстре. computePath работает как Дейкстра (для совместимости с
 * единым интерфейсом). */
class GreedySegmentation : public IRoutingAlgorithm
{
public:
    QString id() const override { return QStringLiteral("greedy_segmentation"); }
    QString displayName() const override { return QStringLiteral("Жадное сегментирование"); }
    QString kind() const override { return QStringLiteral("segments"); }
    bool needsFullRecompute() const override { return true; }

    QList<QList<int>> computeTree(const Graph &graph, int root) override;
    QList<int> computePath(const Graph &graph, int from, int to) override;
    QList<QList<int>> computeSegments(const Graph &graph, int k = 2) override;
};

#endif // GREEDYSEGMENTATION_H
