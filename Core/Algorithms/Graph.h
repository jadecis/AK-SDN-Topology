#ifndef ALGO_GRAPH_H
#define ALGO_GRAPH_H

#include <QVector>
#include <limits>

/* Простой контейнер для матрицы весов n×n.
 * Используется встроенными алгоритмами маршрутизации.
 * weight(i, j) == INF() означает отсутствие ребра. */
class Graph
{
public:
    static inline double INF() { return std::numeric_limits<double>::infinity(); }

    explicit Graph(int n = 0)
        : n_(n), data_(n * n, INF())
    {
        for (int i = 0; i < n; ++i)
            data_[i * n + i] = 0.0;
    }

    int size() const { return n_; }

    void set(int i, int j, double w)
    {
        data_[i * n_ + j] = w;
    }

    double weight(int i, int j) const
    {
        return data_[i * n_ + j];
    }

    bool hasEdge(int i, int j) const
    {
        return i != j && data_[i * n_ + j] < INF();
    }

private:
    int n_;
    QVector<double> data_;
};

#endif // ALGO_GRAPH_H
