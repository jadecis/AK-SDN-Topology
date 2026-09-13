#include "DijkstraAlgorithm.h"

void DijkstraAlgorithm::run(const Graph &graph, int root,
                            QVector<double> &tagsOut, QVector<int> &prevOut) const
{
    const int n = graph.size();
    tagsOut.fill(Graph::INF(), n);
    prevOut.fill(-1, n);
    if (n == 0 || root < 0 || root >= n) return;

    tagsOut[root] = 0.0;
    QVector<bool> visited(n, false);

    for (int iter = 0; iter < n; ++iter)
    {
        /* Выбираем непосещённую вершину с минимальной меткой. */
        int b = -1;
        double bestTag = Graph::INF();
        for (int i = 0; i < n; ++i)
        {
            if (!visited[i] && tagsOut[i] < bestTag)
            {
                bestTag = tagsOut[i];
                b = i;
            }
        }
        if (b < 0 || bestTag == Graph::INF())
        {
            break;
        }

        visited[b] = true;

        /* Релаксация рёбер из b. */
        for (int i = 0; i < n; ++i)
        {
            if (visited[i] || !graph.hasEdge(b, i)) continue;
            double cand = tagsOut[b] + graph.weight(b, i);
            if (cand < tagsOut[i])
            {
                tagsOut[i] = cand;
                prevOut[i] = b;
            }
        }
    }
}

QList<QList<int>> DijkstraAlgorithm::computeTree(const Graph &graph, int root)
{
    QVector<double> tags;
    QVector<int> prev;
    run(graph, root, tags, prev);

    QList<QList<int>> tree;
    const int n = graph.size();
    for (int i = 0; i < n; ++i)
    {
        if (i == root) continue;
        if (prev[i] >= 0)
        {
            tree.append({prev[i], i});
        }
    }
    return tree;
}

QList<int> DijkstraAlgorithm::computePath(const Graph &graph, int from, int to)
{
    QVector<double> tags;
    QVector<int> prev;
    run(graph, from, tags, prev);

    QList<int> path;
    if (to < 0 || to >= graph.size() || tags[to] == Graph::INF())
    {
        return path;
    }
    /* Восстанавливаем от to до from. */
    int cur = to;
    while (cur != -1)
    {
        path.prepend(cur);
        if (cur == from) break;
        cur = prev[cur];
    }
    if (path.isEmpty() || path.first() != from)
    {
        return QList<int>();
    }
    return path;
}
