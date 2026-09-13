#include "LARACAlgorithm.h"
#include "DijkstraAlgorithm.h"

#include <QSet>
#include <QPair>
#include <cmath>

/* === Вспомогательные функции === */

/* Сумма весов графа g вдоль пути (последовательность узлов). INF, если
 * какое-то ребро пути отсутствует. */
static double sumAlong(const Graph &g, const QList<int> &path)
{
    double s = 0.0;
    for (int i = 0; i + 1 < path.size(); ++i)
    {
        double w = g.weight(path[i], path[i + 1]);
        if (w == Graph::INF()) return Graph::INF();
        s += w;
    }
    return s;
}

/* Кратчайший путь по графу g (Дейкстра). */
static QList<int> dijkstraPath(const Graph &g, int from, int to)
{
    DijkstraAlgorithm d;
    return d.computePath(g, from, to);
}

/* Агрегированный граф cost + λ·constr (по общим рёбрам). */
static Graph aggregate(const Graph &cost, const Graph &constr, double lambda)
{
    const int n = cost.size();
    Graph out(n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            if (i == j) continue;
            if (!cost.hasEdge(i, j)) continue;
            double c = cost.weight(i, j);
            double d = constr.hasEdge(i, j) ? constr.weight(i, j) : 0.0;
            out.set(i, j, c + lambda * d);
        }
    return out;
}

/* === Двухкритериальный LARAC === */

QList<int> LARACAlgorithm::computeConstrainedPath(const Graph &cost, const Graph &constr,
                                                  int from, int to, double R2,
                                                  double lambdaFixed) const
{
    if (from == to) return QList<int>{ from };

    /* Ручной режим: фиксированный множитель Лагранжа, один проход. */
    if (lambdaFixed > 0.0)
    {
        Graph agg = aggregate(cost, constr, lambdaFixed);
        return dijkstraPath(agg, from, to);
    }

    /* Авто-LARAC. */
    QList<int> pc = dijkstraPath(cost, from, to);          // минимум по cost
    if (pc.isEmpty()) return pc;
    if (sumAlong(constr, pc) <= R2) return pc;             // уже допустим → оптимум

    QList<int> pd = dijkstraPath(constr, from, to);        // минимум по constr
    if (pd.isEmpty()) return pc;
    if (sumAlong(constr, pd) > R2) return pd;              // допустимого нет → best-effort

    for (int iter = 0; iter < 100; ++iter)
    {
        double cPc = sumAlong(cost, pc),   dPc = sumAlong(constr, pc);
        double cPd = sumAlong(cost, pd),   dPd = sumAlong(constr, pd);
        double denom = dPd - dPc;
        if (std::fabs(denom) < 1e-9) break;
        double lambda = (cPc - cPd) / denom;
        if (lambda < 0.0) lambda = 0.0;

        Graph agg = aggregate(cost, constr, lambda);
        QList<int> r = dijkstraPath(agg, from, to);
        if (r.isEmpty()) break;

        double aggR  = sumAlong(agg, r);
        double aggPc = cPc + lambda * dPc;
        if (aggR >= aggPc - 1e-9)        // улучшения нет
            return pd;

        if (sumAlong(constr, r) <= R2)
            pd = r;
        else
            pc = r;
    }
    return pd;
}

QList<QList<int>> LARACAlgorithm::computeConstrainedTree(const Graph &cost, const Graph &constr,
                                                         int root, double R2,
                                                         double lambdaFixed) const
{
    QList<QList<int>> tree;
    const int n = cost.size();
    if (root < 0 || root >= n) return tree;

    QSet<QPair<int, int>> seen;   // дедуп рёбер (нормализованных)
    for (int t = 0; t < n; ++t)
    {
        if (t == root) continue;
        QList<int> p = computeConstrainedPath(cost, constr, root, t, R2, lambdaFixed);
        for (int i = 0; i + 1 < p.size(); ++i)
        {
            int a = p[i], b = p[i + 1];
            QPair<int, int> key = a < b ? qMakePair(a, b) : qMakePair(b, a);
            if (seen.contains(key)) continue;
            seen.insert(key);
            tree.append({ a, b });
        }
    }
    return tree;
}

/* === Fallback (один граф): ведём себя как Дейкстра по переданной метрике ===
 * Используется только если вторая метрика недоступна. */

QList<QList<int>> LARACAlgorithm::computeTree(const Graph &graph, int root)
{
    DijkstraAlgorithm dij;
    return dij.computeTree(graph, root);
}

QList<int> LARACAlgorithm::computePath(const Graph &graph, int from, int to)
{
    DijkstraAlgorithm dij;
    return dij.computePath(graph, from, to);
}
