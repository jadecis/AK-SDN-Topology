#include "PairTransitionAlgorithm.h"
#include "DijkstraAlgorithm.h"

QList<QList<int>> PairTransitionAlgorithm::computeTree(const Graph &graph, int root)
{
    /* Локальная визуализация: ДОМ совпадает с деревом Дейкстры.
     * Подсветка парных каналов отрисовывается отдельно — через TCP
     * сообщение от Ryu, когда сервер посчитает backup_pairs. */
    DijkstraAlgorithm d;
    return d.computeTree(graph, root);
}

QList<int> PairTransitionAlgorithm::computePath(const Graph &graph, int from, int to)
{
    DijkstraAlgorithm d;
    return d.computePath(graph, from, to);
}
