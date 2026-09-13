#ifndef PAIRTRANSITIONALGORITHM_H
#define PAIRTRANSITIONALGORITHM_H

#include "IRoutingAlgorithm.h"

/* Алгоритм парных переходов — глава 6 пособия Перепёлкина Д.А.
 *
 * Идея алгоритма:
 *   1. Строится ДОМ (дерево оптимальных маршрутов) Дейкстрой от корня S0.
 *   2. Для каждого ребра ДОМ заранее вычисляется потенциальная пара —
 *      резервный канал, на который трафик мгновенно переключится, если
 *      метрика основного канала превысит точку выхода Tij.
 *   3. При изменении метрики не нужен полный пересчёт Дейкстры —
 *      достаточно проверить пороги Tij/Eij и сделать парный переход
 *      за O(1).
 *
 * Локальная C++-реализация считает ДОМ (как обычная Дейкстра) и эмитит
 * его на канвас — пользователь видит структуру дерева. Полный расчёт
 * парных каналов и динамическое переключение производится на стороне
 * Ryu (см. PairTransitionsAlgo в resources/templates/ryu/algorithms.py),
 * куда SNet шлёт запрос при включённой активной маршрутизации.
 *
 * Для визуализации этого достаточно: ДОМ совпадает с Дейкстрой, а
 * backup_pairs приходит из Ryu через TCP-канал визуализации. */
class PairTransitionAlgorithm : public IRoutingAlgorithm
{
public:
    QString id() const override { return QStringLiteral("pair_transitions"); }
    QString displayName() const override { return QStringLiteral("Парные переходы"); }

    QList<QList<int>> computeTree(const Graph &graph, int root) override;
    QList<int> computePath(const Graph &graph, int from, int to) override;
};

#endif // PAIRTRANSITIONALGORITHM_H
