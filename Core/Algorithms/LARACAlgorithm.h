#ifndef LARACALGORITHM_H
#define LARACALGORITHM_H

#include "IRoutingAlgorithm.h"

/* LARAC (Lagrange Relaxation Aggregated Cost) — поиск маршрута, минимального
 * по 1-й метрике (cost) при ограничении по 2-й метрике (constr) ≤ R₂.
 *
 * Классическая схема (Jüttner и др.):
 *   pc = кратчайший по cost. Если constr(pc) ≤ R₂ — он же оптимален.
 *   pd = кратчайший по constr. Если constr(pd) > R₂ — допустимого пути нет,
 *        возвращаем pd (наименьшие потери, best-effort).
 *   Иначе итеративно подбираем λ = (cost(pc)−cost(pd)) / (constr(pd)−constr(pc)),
 *   ищем кратчайший по агрегированному весу cost + λ·constr и сдвигаем pc/pd,
 *   пока агрегированная стоимость не перестанет улучшаться.
 *
 * λ_fixed > 0 — «ручной» режим: один проход Дейкстры по cost + λ·constr
 * (демонстрация множителя Лагранжа без подбора). λ_fixed = 0 — авто-LARAC.
 *
 * Для tree-режима дерево строится объединением LARAC-путей от root до всех
 * остальных узлов.
 *
 * Базовые computeTree/computePath (один граф) оставлены как fallback: когда
 * вторая метрика недоступна, ведут себя как Дейкстра по переданной метрике. */
class LARACAlgorithm : public IRoutingAlgorithm
{
public:
    QString id() const override { return QStringLiteral("larac"); }
    QString displayName() const override { return QStringLiteral("LARAC"); }

    QList<QList<int>> computeTree(const Graph &graph, int root) override;
    QList<int> computePath(const Graph &graph, int from, int to) override;

    /* Двухкритериальный LARAC: cost — 1-я метрика, constr — 2-я метрика,
     * R2 — ограничение по 2-й метрике, lambdaFixed — см. описание класса. */
    QList<int> computeConstrainedPath(const Graph &cost, const Graph &constr,
                                      int from, int to, double R2,
                                      double lambdaFixed = 0.0) const;
    QList<QList<int>> computeConstrainedTree(const Graph &cost, const Graph &constr,
                                             int root, double R2,
                                             double lambdaFixed = 0.0) const;
};

#endif // LARACALGORITHM_H
