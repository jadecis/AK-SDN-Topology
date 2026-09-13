"""
SNet — базовый класс для алгоритмов маршрутизации.

Каждый алгоритм (как встроенный, так и пользовательский) должен наследовать
от BaseAlgorithm и реализовать метод run().

ИНТЕРФЕЙС:

    class MyAlgo(BaseAlgorithm):
        name = "Имя алгоритма"               # отображается в UI и веб-интерфейсе
        description = "Краткое описание"      # подсказка
        params = {                            # параметры, которые задаются пользователем
            "k":   ("int",   3, "Количество альтернативных путей"),
            "lambda": ("float", 0.5, "Коэффициент LARAC"),
        }

        def run(self, src, dst, graphs, params):
            self.log("Стартую алгоритм")
            # graphs['delay']     — словарь смежности по задержке     {sw: {sw: val}}
            # graphs['loss']      — словарь смежности по потерям
            # graphs['bandwidth'] — словарь смежности по пропускной способности
            # params['k']         — пользовательский параметр
            #
            # вернуть нужно dict:
            return {
                "routes":  [[1, 2, 3, 7]],   # список маршрутов (списки номеров sw)
                "tree":    [[1, 2], [2, 3]], # рёбра дерева, опционально
                "segments":[[1, 2], [3, 4]], # сегменты сети, опционально
                "cost":    42.0,             # стоимость основного маршрута
            }

ВИЗУАЛИЗАЦИЯ:
    После выполнения run() веб-интерфейс автоматически отправит routes/tree/segments
    в SDN Topology через TCP (см. sdn_topology.py) и обновит подсветку на канве.
"""


class BaseAlgorithm:
    name = "Unnamed"
    description = ""
    params = {}

    def __init__(self):
        self._log_lines = []

    def log(self, *args):
        """Записывает строку в журнал алгоритма. Видна на веб-странице /algorithm."""
        msg = " ".join(str(a) for a in args)
        self._log_lines.append(msg)
        try:
            import sys
            sys.stdout.write("[ALG] " + msg + "\n")
            sys.stdout.flush()
        except Exception:
            pass

    def get_log(self):
        return "\n".join(self._log_lines)

    def reset_log(self):
        self._log_lines = []

    def run(self, src, dst, graphs, params):
        """Должен быть переопределён в наследнике.

        Аргументы:
          src     — начальный коммутатор (int, id из топологии)
          dst     — конечный коммутатор (int)
          graphs  — dict из словарей смежности: {"delay": {...}, "loss": {...}, "bandwidth": {...}}
          params  — dict {имя_параметра: значение}

        Должен вернуть dict с полями routes / tree / segments / cost (любые из них).
        """
        raise NotImplementedError("Algorithm subclass must implement run()")
