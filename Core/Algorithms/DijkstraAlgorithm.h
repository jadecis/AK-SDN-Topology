#ifndef DIJKSTRAALGORITHM_H
#define DIJKSTRAALGORITHM_H

#include "IRoutingAlgorithm.h"

/* Алгоритм Дейкстры: одна вершина → все.
 * Реализация по §3.2.1 пособия Перепёлкина Д.А.:
 *   • инициализация меток (tags) бесконечностями, tags[root] = 0;
 *   • на каждом шаге берём непосещённую вершину с минимальной меткой,
 *     релаксируем рёбра, добавляем в visited;
 *   • prev[i] хранит предыдущий узел в оптимальном маршруте от root до i.
 *
 * computeTree возвращает рёбра ДОМ как [prev[i], i] для всех достижимых i. */
class DijkstraAlgorithm : public IRoutingAlgorithm
{
public:
    QString id() const override { return QStringLiteral("dijkstra"); }
    QString displayName() const override { return QStringLiteral("Дейкстра"); }

    QList<QList<int>> computeTree(const Graph &graph, int root) override;
    QList<int> computePath(const Graph &graph, int from, int to) override;

private:
    /* Возвращает (tags, prev) — длины кратчайших путей от root и предков. */
    void run(const Graph &graph, int root,
             QVector<double> &tagsOut, QVector<int> &prevOut) const;
};

#endif // DIJKSTRAALGORITHM_H
