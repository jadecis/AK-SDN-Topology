"""
SNet — встроенные алгоритмы маршрутизации для Ryu-контроллера.

Алгоритмы делятся на:
  • классические:        Dijkstra, BellmanFord
  • QoS-маршрутизация:   MCP, MCOP, CSP, LARAC (см. главу 5 пособия)
  • сегментирование:     GreedySegmentation (см. главу 7 пособия)
  • парные переходы:     PairTransitions (см. главу 6 пособия)

Все они — подклассы BaseAlgorithm (см. base_algorithm.py).
Веб-интерфейс автоматически отображает их в выпадающем списке.

Алгоритмы пользователя — в папке algorithms_user/.
"""

import copy
import math
import heapq

from base_algorithm import BaseAlgorithm


# ====================================================================
# вспомогательные функции (используются и встроенными, и пользовательскими)
# ====================================================================

def get_all_paths(graph, start, end, path=None, max_paths=200):
    """Все простые пути от start до end. Ограничение по числу путей —
    защита от комбинаторного взрыва на больших топологиях."""
    if path is None:
        path = []
    path = path + [start]
    if start == end:
        return [path]
    if start not in graph:
        return []
    paths = []
    for node in graph[start]:
        if node not in path:
            new_paths = get_all_paths(graph, node, end, path, max_paths)
            for p in new_paths:
                paths.append(p)
                if len(paths) >= max_paths:
                    return paths
    return paths


def path_cost(path, graph):
    if not path or len(path) < 2:
        return 0
    cost = 0
    for i in range(len(path) - 1):
        a, b = path[i], path[i + 1]
        if a in graph and b in graph[a]:
            cost += graph[a][b]
        else:
            return float("inf")
    return cost


def dijkstra_paths(graph, src):
    """Возвращает (dist, prev) — кратчайшие пути от src до всех остальных."""
    dist = {n: math.inf for n in graph}
    prev = {n: None for n in graph}
    dist[src] = 0
    pq = [(0, src)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist[u]:
            continue
        for v, w in graph.get(u, {}).items():
            nd = d + w
            if nd < dist[v]:
                dist[v] = nd
                prev[v] = u
                heapq.heappush(pq, (nd, v))
    return dist, prev


def recover_path(prev, src, dst):
    if dst not in prev or prev[dst] is None and src != dst:
        return []
    path = []
    cur = dst
    while cur is not None:
        path.append(cur)
        if cur == src:
            break
        cur = prev[cur]
    if not path or path[-1] != src:
        return []
    return list(reversed(path))


# ====================================================================
# Классические алгоритмы
# ====================================================================

class DijkstraAlgo(BaseAlgorithm):
    name = "Дейкстра"
    description = "Кратчайший путь по выбранной метрике (delay/loss/bandwidth)"
    params = {
        "metric": ("str", "delay", "Метрика веса: delay | loss | bandwidth"),
    }

    def run(self, src, dst, graphs, params):
        metric = params.get("metric", "delay")
        graph = graphs.get(metric, graphs.get("delay", {}))
        self.log("Запуск Дейкстры от", src, "до", dst, "по метрике", metric)
        dist, prev = dijkstra_paths(graph, src)
        path = recover_path(prev, src, dst)
        self.log("Расстояния:", dict(dist))
        if not path:
            self.log("Путь не найден")
            return {"routes": []}
        cost = dist[dst]
        self.log("Найден маршрут:", path, "стоимость =", cost)
        # дерево от src до всех достижимых
        tree = []
        for node, p in prev.items():
            if p is not None:
                tree.append([p, node])
        return {"routes": [path], "tree": tree, "cost": cost}


class BellmanFordAlgo(BaseAlgorithm):
    name = "Беллман–Форд"
    description = "Поддерживает отрицательные веса. O(V·E)."
    params = {
        "metric": ("str", "delay", "Метрика веса"),
    }

    def run(self, src, dst, graphs, params):
        metric = params.get("metric", "delay")
        graph = graphs.get(metric, {})
        self.log("Беллман–Форд от", src, "до", dst, "метрика", metric)
        nodes = list(graph.keys())
        dist = {n: math.inf for n in nodes}
        prev = {n: None for n in nodes}
        dist[src] = 0
        for _ in range(len(nodes) - 1):
            for u in nodes:
                for v, w in graph.get(u, {}).items():
                    if dist[u] + w < dist[v]:
                        dist[v] = dist[u] + w
                        prev[v] = u
        path = recover_path(prev, src, dst)
        if not path:
            return {"routes": []}
        return {"routes": [path], "cost": dist[dst]}


# ====================================================================
# QoS-маршрутизация (глава 5 пособия)
# ====================================================================

class MCPAlgo(BaseAlgorithm):
    name = "MCP — Multi-Constrained Path"
    description = "Все пути, у которых стоимости по двум метрикам ≤ ограничений."
    params = {
        "metric1":     ("str",   "delay", "Первая метрика"),
        "metric2":     ("str",   "loss",  "Вторая метрика"),
        "restriction1": ("float", 50,     "Ограничение по первой метрике"),
        "restriction2": ("float", 5,      "Ограничение по второй метрике"),
    }

    def run(self, src, dst, graphs, params):
        m1 = params.get("metric1", "delay")
        m2 = params.get("metric2", "loss")
        r1 = float(params.get("restriction1", 50))
        r2 = float(params.get("restriction2", 5))
        g1 = graphs.get(m1, {})
        g2 = graphs.get(m2, {})

        self.log(f"MCP {m1}≤{r1} и {m2}≤{r2}")
        all_paths = get_all_paths(g1, src, dst)
        self.log("Всего простых путей:", len(all_paths))
        routes = []
        for p in all_paths:
            c1 = path_cost(p, g1)
            c2 = path_cost(p, g2)
            if c1 <= r1 and c2 <= r2:
                routes.append(p)
                self.log(f"  принят: {p}  {m1}={c1}  {m2}={c2}")
        self.log("Прошли фильтр:", len(routes))
        return {"routes": routes}


class MCOPAlgo(BaseAlgorithm):
    name = "MCOP — Multi-Constrained Optimal Path"
    description = "Лучший путь по m1, среди равных — лучший по m2."
    params = {
        "metric1": ("str", "delay", "Главная метрика"),
        "metric2": ("str", "loss",  "Вторичная метрика"),
    }

    def run(self, src, dst, graphs, params):
        m1 = params.get("metric1", "delay")
        m2 = params.get("metric2", "loss")
        g1 = graphs.get(m1, {})
        g2 = graphs.get(m2, {})

        all_paths = get_all_paths(g1, src, dst)
        if not all_paths:
            return {"routes": []}
        best_cost1 = float("inf")
        best = []
        for p in all_paths:
            c1 = path_cost(p, g1)
            if c1 < best_cost1:
                best_cost1 = c1
                best = [p]
            elif c1 == best_cost1:
                best.append(p)
        self.log(f"Лучших по {m1}: {len(best)} (стоимость {best_cost1})")
        final = None
        best_c2 = float("inf")
        for p in best:
            c2 = path_cost(p, g2)
            if c2 < best_c2:
                best_c2 = c2
                final = p
        self.log(f"Финальный: {final}, {m1}={best_cost1}, {m2}={best_c2}")
        return {"routes": [final] if final else []}


class CSPAlgo(BaseAlgorithm):
    name = "CSP — Constrained Shortest Path"
    description = "Минимизирует одну метрику при ограничении на другую."
    params = {
        "opt_metric":     ("str",   "loss",  "Метрика для минимизации"),
        "restrict_metric": ("str",   "delay", "Метрика-ограничение"),
        "restriction":    ("float", 10,     "Значение ограничения"),
    }

    def run(self, src, dst, graphs, params):
        opt_m = params.get("opt_metric", "loss")
        res_m = params.get("restrict_metric", "delay")
        rest = float(params.get("restriction", 10))
        g_opt = graphs.get(opt_m, {})
        g_res = graphs.get(res_m, {})

        self.log(f"CSP: min({opt_m}) s.t. {res_m}≤{rest}")
        all_paths = get_all_paths(g_opt, src, dst)
        valid = [p for p in all_paths if path_cost(p, g_res) <= rest]
        self.log("Допустимых маршрутов:", len(valid))
        if not valid:
            return {"routes": []}
        best = min(valid, key=lambda p: path_cost(p, g_opt))
        self.log("Лучший:", best,
                 f"{opt_m}={path_cost(best, g_opt)}",
                 f"{res_m}={path_cost(best, g_res)}")
        return {"routes": [best], "cost": path_cost(best, g_opt)}


class LARACAlgo(BaseAlgorithm):
    name = "LARAC"
    description = "Приближённый CSP через лагранжеву релаксацию."
    params = {
        "metric1": ("str",   "delay", "Основная метрика"),
        "metric2": ("str",   "loss",  "Метрика-ограничение"),
        "restriction1": ("float", 30, "Бюджет по m1"),
        "restriction2": ("float", 10, "Бюджет по m2"),
        "lambd":   ("float", 0.5,    "Лагранжев коэффициент"),
    }

    def run(self, src, dst, graphs, params):
        m1 = params.get("metric1", "delay")
        m2 = params.get("metric2", "loss")
        r1 = float(params.get("restriction1", 30))
        r2 = float(params.get("restriction2", 10))
        lam = float(params.get("lambd", 0.5))
        g1 = graphs.get(m1, {})
        g2 = graphs.get(m2, {})

        all_paths = get_all_paths(g1, src, dst)

        def agg(p):
            return path_cost(p, g1) + lam * path_cost(p, g2)

        agg_lim = r1 + lam * r2
        valid = [p for p in all_paths if agg(p) <= agg_lim]
        self.log(f"LARAC: λ={lam}, лимит_агг={agg_lim}, прошли: {len(valid)}")
        if not valid:
            return {"routes": []}
        best = min(valid, key=agg)
        self.log("Лучший:", best, "агр.стоимость=", agg(best))
        return {"routes": [best]}


# ====================================================================
# Парные переходы (глава 6 пособия)
# ====================================================================

class PairTransitionsAlgo(BaseAlgorithm):
    name = "Парные переходы"
    description = "Адаптивное дерево оптимальных маршрутов с резервными парами."
    params = {
        "metric": ("str", "delay", "Метрика веса"),
    }

    def run(self, src, dst, graphs, params):
        metric = params.get("metric", "delay")
        graph = graphs.get(metric, {})
        self.log("Парные переходы: формирование ДОМ от", src)
        _, prev = dijkstra_paths(graph, src)
        tree_edges = [[prev[n], n] for n in prev if prev[n] is not None]
        self.log("ДОМ:", tree_edges)

        # Для каждого ребра ДОМ ищем потенциальный парный канал замены.
        backup_pairs = []
        in_tree = {(a, b) for a, b in tree_edges} | {(b, a) for a, b in tree_edges}
        for a, b in tree_edges:
            # candidates — все рёбра, инцидентные b, кроме (a,b) и кроме рёбер ДОМ
            candidates = []
            for nb in graph.get(b, {}):
                if nb == a:
                    continue
                if (b, nb) in in_tree:
                    continue
                candidates.append([b, nb])
            if candidates:
                backup_pairs.append({"in_tree": [a, b], "candidates": candidates})
                self.log(f"  Канал {a}-{b}: возможные пары {candidates}")
        if dst is not None and src is not None:
            path = recover_path(prev, src, dst)
            return {"routes": [path] if path else [], "tree": tree_edges,
                    "backup_pairs": backup_pairs}
        return {"tree": tree_edges, "backup_pairs": backup_pairs}


# ====================================================================
# Сегментирование (глава 7 пособия)
# ====================================================================

class GreedySegmentationAlgo(BaseAlgorithm):
    name = "Жадное сегментирование"
    description = "Разбивает топологию на k сегментов жадно по связности."
    params = {
        "k": ("int", 2, "Количество сегментов"),
    }

    def run(self, src, dst, graphs, params):
        k = int(params.get("k", 2))
        graph = graphs.get("delay", {})
        nodes = list(graph.keys())
        if k <= 0 or not nodes:
            return {"segments": []}
        self.log(f"Жадное сегментирование на {k} частей")
        # Стартуем с самых "удалённых" друг от друга узлов (приблизительно).
        seeds = []
        used = set()
        # первый seed — самый связный
        deg = {n: len(graph.get(n, {})) for n in nodes}
        first = max(nodes, key=lambda n: deg[n])
        seeds.append(first)
        used.add(first)
        # остальные seeds — максимизируем мин. расстояние до уже выбранных
        for _ in range(k - 1):
            best = None
            best_dist = -1
            for n in nodes:
                if n in used:
                    continue
                d = min((path_cost(recover_path(dijkstra_paths(graph, s)[1], s, n), graph)
                         or float("inf")) for s in seeds)
                if d != float("inf") and d > best_dist:
                    best_dist = d
                    best = n
            if best is None:
                break
            seeds.append(best)
            used.add(best)
        # Назначаем каждый узел ближайшему seed'у
        segments = [[] for _ in seeds]
        for n in nodes:
            best_s = 0
            best_d = float("inf")
            for i, s in enumerate(seeds):
                _, prev = dijkstra_paths(graph, s)
                p = recover_path(prev, s, n)
                d = path_cost(p, graph) if p else float("inf")
                if d < best_d:
                    best_d = d
                    best_s = i
            segments[best_s].append(n)
        self.log("Сегменты:", segments)
        return {"segments": segments}


# ====================================================================
# Регистрация встроенных алгоритмов
# ====================================================================

BUILTIN_ALGORITHMS = [
    DijkstraAlgo,
    BellmanFordAlgo,
    MCPAlgo,
    MCOPAlgo,
    CSPAlgo,
    LARACAlgo,
    PairTransitionsAlgo,
    GreedySegmentationAlgo,
]
