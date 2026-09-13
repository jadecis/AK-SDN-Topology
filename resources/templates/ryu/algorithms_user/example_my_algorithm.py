"""
ПРИМЕР пользовательского алгоритма.

Чтобы создать свой алгоритм:
  1. Скопируйте этот файл, переименуйте (например, my_dijkstra.py).
  2. Измените имя класса (например, class MyDijkstra(BaseAlgorithm)).
  3. Поменяйте поле name — оно будет в выпадающем списке веб-интерфейса.
  4. Реализуйте свою логику в методе run().
  5. На веб-странице нажмите «Перезагрузить алгоритмы» (/reload),
     либо перезапустите контроллер.

Документация интерфейса — в base_algorithm.py.
"""

from base_algorithm import BaseAlgorithm


class ExampleMyAlgorithm(BaseAlgorithm):
    """Пример: возвращает кратчайший путь по delay через простой BFS.
    Это упрощённый Дейкстра, использующий равные веса — просто чтобы
    показать, как написать собственный алгоритм."""

    name = "Пример — BFS-маршрутизация"
    description = ("Демонстрационный алгоритм. Считает путь от src до dst "
                   "методом поиска в ширину по графу delay.")
    params = {
        "metric": ("str", "delay", "Метрика для построения графа"),
    }

    def run(self, src, dst, graphs, params):
        metric = params.get("metric", "delay")
        graph = graphs.get(metric, {})
        self.log(f"BFS от {src} до {dst} по графу '{metric}'")
        self.log(f"Соседей у {src}: {list(graph.get(src, {}).keys())}")

        # BFS
        visited = {src}
        queue = [(src, [src])]
        while queue:
            node, path = queue.pop(0)
            if node == dst:
                self.log(f"Найден путь длины {len(path)}: {path}")
                # Стоимость по метрике
                cost = 0
                for i in range(len(path) - 1):
                    a, b = path[i], path[i + 1]
                    cost += graph.get(a, {}).get(b, 0)
                return {
                    "routes": [path],
                    "cost": cost,
                }
            for neighbour in graph.get(node, {}):
                if neighbour not in visited:
                    visited.add(neighbour)
                    queue.append((neighbour, path + [neighbour]))

        self.log("Путь не найден")
        return {"routes": []}
