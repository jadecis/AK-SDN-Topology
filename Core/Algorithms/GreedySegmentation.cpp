#include "GreedySegmentation.h"
#include "DijkstraAlgorithm.h"

#include <QVector>
#include <limits>

QList<QList<int>> GreedySegmentation::computeTree(const Graph &graph, int root)
{
    /* В режиме «дерево» отдаём SPT Дейкстры для совместимости. */
    DijkstraAlgorithm dij;
    return dij.computeTree(graph, root);
}

QList<int> GreedySegmentation::computePath(const Graph &graph, int from, int to)
{
    DijkstraAlgorithm dij;
    return dij.computePath(graph, from, to);
}

/* Жадное сегментирование (§7.3.2 пособия):
 *   1. Выбираем k «центров» сегментов — узлы с максимальной суммарной
 *      связностью (по убыванию степени). Берём по одному с шагом, чтобы
 *      они были «разнесены».
 *   2. Для каждого остального узла считаем расстояние Дейкстры до каждого
 *      центра и относим к ближайшему.
 *   3. Возвращаем k списков индексов.
 *
 * Это даёт визуально различимые «острова» — что и требуется методичкой. */
QList<QList<int>> GreedySegmentation::computeSegments(const Graph &graph, int k)
{
    const int n = graph.size();
    QList<QList<int>> segs;
    if (n == 0) return segs;
    if (k < 1) k = 1;
    if (k > n) k = n;

    /* Степень каждого узла. */
    QVector<int> degree(n, 0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            if (i != j && graph.hasEdge(i, j)) degree[i]++;

    /* Выбираем k центров — по индексу, разнесённых равномерно по графу. */
    QVector<int> centers;
    QVector<bool> taken(n, false);
    /* Первый центр — узел с максимальной степенью. */
    int best = 0;
    for (int i = 1; i < n; ++i) if (degree[i] > degree[best]) best = i;
    centers.append(best);
    taken[best] = true;

    /* Остальные k-1 центров: каждый максимально удалён от уже выбранных. */
    DijkstraAlgorithm dij;
    QVector<QVector<double>> distFromCenter;
    {
        Graph g = graph;
        Q_UNUSED(g);
    }
    for (int c = 1; c < k; ++c)
    {
        /* Расстояния от каждого уже выбранного центра до всех узлов. */
        QVector<double> minDistToCenters(n, std::numeric_limits<double>::infinity());
        for (int cIdx : centers)
        {
            QList<int> nothing;
            QVector<double> d(n, Graph::INF());
            QVector<int> prev(n, -1);
            /* Используем computePath в роли «расстояния»: запускаем для каждого */
            /* узла отдельный computePath — это N^2 log N, но N мало. */
            /* Проще — переиспользовать алгоритм Дейкстры через свой раннер: */
            QVector<bool> visited(n, false);
            d[cIdx] = 0.0;
            for (int iter = 0; iter < n; ++iter)
            {
                int u = -1; double best_ = Graph::INF();
                for (int i = 0; i < n; ++i)
                    if (!visited[i] && d[i] < best_) { best_ = d[i]; u = i; }
                if (u < 0) break;
                visited[u] = true;
                for (int v = 0; v < n; ++v)
                {
                    if (visited[v] || !graph.hasEdge(u, v)) continue;
                    double cand = d[u] + graph.weight(u, v);
                    if (cand < d[v]) d[v] = cand;
                }
            }
            for (int i = 0; i < n; ++i)
                if (d[i] < minDistToCenters[i]) minDistToCenters[i] = d[i];
        }
        /* Берём узел с максимальным минимальным расстоянием. */
        int nextCenter = -1;
        double bestDist = -1.0;
        for (int i = 0; i < n; ++i)
        {
            if (taken[i]) continue;
            if (minDistToCenters[i] != Graph::INF() &&
                minDistToCenters[i] > bestDist)
            {
                bestDist = minDistToCenters[i];
                nextCenter = i;
            }
        }
        if (nextCenter < 0)
        {
            /* Граф несвязный — берём первый непокрытый. */
            for (int i = 0; i < n; ++i) if (!taken[i]) { nextCenter = i; break; }
            if (nextCenter < 0) break;
        }
        centers.append(nextCenter);
        taken[nextCenter] = true;
    }

    /* 2. Каждый узел относим к ближайшему центру (по Дейкстре). */
    QVector<int> assignment(n, 0);
    for (int v = 0; v < n; ++v)
    {
        double bestDist = std::numeric_limits<double>::infinity();
        int bestCenterIdx = 0;
        for (int ci = 0; ci < centers.size(); ++ci)
        {
            int c = centers[ci];
            /* Считаем расстояние Дейкстрой от центра c до v. */
            QVector<double> d(n, Graph::INF());
            QVector<bool> visited(n, false);
            d[c] = 0.0;
            for (int iter = 0; iter < n; ++iter)
            {
                int u = -1; double best_ = Graph::INF();
                for (int i = 0; i < n; ++i)
                    if (!visited[i] && d[i] < best_) { best_ = d[i]; u = i; }
                if (u < 0) break;
                if (u == v) break;
                visited[u] = true;
                for (int w = 0; w < n; ++w)
                {
                    if (visited[w] || !graph.hasEdge(u, w)) continue;
                    double cand = d[u] + graph.weight(u, w);
                    if (cand < d[w]) d[w] = cand;
                }
            }
            if (d[v] < bestDist)
            {
                bestDist = d[v];
                bestCenterIdx = ci;
            }
        }
        assignment[v] = bestCenterIdx;
    }

    /* 3. Группируем по центрам. */
    segs.clear();
    for (int ci = 0; ci < centers.size(); ++ci) segs.append(QList<int>());
    for (int v = 0; v < n; ++v) segs[assignment[v]].append(v);
    return segs;
}
