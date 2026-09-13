"""
SNet — REST endpoints для Ryu контроллера.

Веб-интерфейс доступен на http://localhost:8080
Endpoints:
    GET /                          — главная (HTML)
    GET /algorithms/list           — JSON список алгоритмов
    GET /algorithm?name=...&src=...&dst=...&<param>=<val>...  — запуск
    GET /topology                  — JSON: коммутаторы, линки, словарь смежности
    GET /log                       — последние 500 строк лога контроллера
    GET /reload                    — перезагрузить пользовательские алгоритмы
"""

import os
import sys
import json
import traceback
import importlib
import importlib.util
import inspect
import pathlib

from ryu.app.wsgi import route, ControllerBase
# Response: в одних версиях ryu экспортируется из wsgi, в других — нет.
try:
    from ryu.app.wsgi import Response
except ImportError:
    from webob import Response
from ryu.lib import hub

import algorithms as builtin_algos
import sdn_topology
from base_algorithm import BaseAlgorithm


# ============================================================
# Plugin loader
# ============================================================

USER_ALGO_DIR = "algorithms_user"


def _discover_classes(module):
    """Вернуть все классы в модуле, которые являются подклассами BaseAlgorithm
    (но не самим BaseAlgorithm)."""
    found = []
    for name, obj in inspect.getmembers(module, inspect.isclass):
        if obj is BaseAlgorithm:
            continue
        if issubclass(obj, BaseAlgorithm) and obj.__module__ == module.__name__:
            found.append(obj)
    return found


def load_all_algorithms(user_dir):
    """Загружает встроенные + пользовательские алгоритмы.

    Возвращает dict {name: class}. При коллизии имён пользовательский имеет
    приоритет (можно переопределить встроенный)."""
    result = {}
    # 1. Встроенные
    for cls in builtin_algos.BUILTIN_ALGORITHMS:
        result[cls.name] = cls

    # 2. Пользовательские
    user_path = pathlib.Path(user_dir)
    if not user_path.is_dir():
        return result

    if str(user_path) not in sys.path:
        sys.path.insert(0, str(user_path))

    for py_file in sorted(user_path.glob("*.py")):
        if py_file.name.startswith("_"):
            continue
        mod_name = "user_" + py_file.stem
        try:
            # Перезагрузка, если уже импортировано
            spec = importlib.util.spec_from_file_location(mod_name, str(py_file))
            mod = importlib.util.module_from_spec(spec)
            sys.modules[mod_name] = mod
            spec.loader.exec_module(mod)
            classes = _discover_classes(mod)
            for cls in classes:
                cls.is_user = True
                cls.source_file = py_file.name
                result[cls.name] = cls
                sys.stdout.write(f"[plugin] загружен {cls.name} из {py_file.name}\n")
        except Exception:
            sys.stderr.write(f"[plugin] ошибка загрузки {py_file}:\n")
            sys.stderr.write(traceback.format_exc())

    return result


# ============================================================
# REST controller
# ============================================================

class ControllerRest(ControllerBase):

    # Кеш: последний запуск алгоритма, чтобы /log без параметров что-то показывал.
    _last_run = {
        "name": None,
        "log": "",
        "result": None,
        "params": None,
        "error": None,
    }


    def __init__(self, req, link, data, **config):
        super(ControllerRest, self).__init__(req, link, data, **config)
        self.controller = data["controller"]
        # Папка проекта (там лежит ryu/, metrics.txt, controller.py).
        # Определяем по пути к этому файлу: <project>/ryu/ryu_app_rest_routing.py
        self.ryu_dir = os.path.dirname(os.path.abspath(__file__))
        self.project_dir = os.path.dirname(self.ryu_dir)
        self.user_dir = os.path.join(self.ryu_dir, USER_ALGO_DIR)
        self.metrics_path = os.path.join(self.project_dir, "metrics.txt")
        self.algorithms = load_all_algorithms(self.user_dir)
        sys.stdout.write(f"[rest] загружено {len(self.algorithms)} алгоритмов\n")

    # ----------------------------------------------------------------
    # Утилиты
    # ----------------------------------------------------------------

    def _build_graphs(self):
        """Строит словари смежности из metrics.txt текущего проекта.
        Если файла нет — возвращает граф, выведенный из switches_reachability контроллера."""
        size = max(self.controller.switches.keys(), default=0)
        if os.path.exists(self.metrics_path):
            return sdn_topology.read_matrix(size, self.metrics_path)
        # Fallback: единичные веса по тому, что видит контроллер
        graphs = {"delay": {}, "bandwidth": {}, "loss": {}}
        for s1, neighbours in self.controller.switches_reachability.items():
            for s2 in neighbours:
                graphs["delay"].setdefault(s1, {})[s2] = 1
                graphs["bandwidth"].setdefault(s1, {})[s2] = 100
                graphs["loss"].setdefault(s1, {})[s2] = 0
        return graphs

    def _switches_reachability_formation(self):
        """Формирует controller.switches_reachability через get_link."""
        try:
            links = self.controller.get_links()
        except Exception:
            return
        for link in links:
            src = link.src.dpid
            dst = link.dst.dpid
            port = link.src.port_no
            self.controller.switches_reachability.setdefault(src, {})[dst] = port

    def _json(self, code, payload):
        body = json.dumps(payload, default=str, ensure_ascii=False)
        return Response(content_type="application/json; charset=utf-8",
                        body=body, status=code)

    def _html(self, body, title="SDN Topology Controller"):
        page = f"""<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><title>{title}</title>
<style>
body {{ font-family: 'Inter','Segoe UI',sans-serif; margin: 0; padding: 20px;
       background: #0F172A; color: #E2E8F0; }}
h1, h2, h3 {{ color: #5BA0FF; }}
a {{ color: #80B6FF; }}
.card {{ background: #1E293B; border: 1px solid #334155;
          border-radius: 8px; padding: 16px; margin-bottom: 16px; }}
table {{ border-collapse: collapse; width: 100%; margin: 8px 0; }}
th, td {{ padding: 6px 12px; text-align: left;
          border-bottom: 1px solid #334155; }}
th {{ color: #5BA0FF; }}
pre {{ background: #0F172A; padding: 12px; border-radius: 6px;
        overflow-x: auto; font-family: 'JetBrains Mono', 'Cascadia Mono', monospace;
        font-size: 12px; color: #94A3B8; }}
form {{ display: grid; grid-template-columns: auto 1fr; gap: 8px 12px;
        max-width: 600px; align-items: center; }}
input, select, button {{ background: #0F172A; color: #E2E8F0;
        border: 1px solid #334155; border-radius: 4px; padding: 6px 10px;
        font-family: inherit; font-size: 13px; }}
button {{ background: #5BA0FF; color: #0F172A; font-weight: 600;
         cursor: pointer; border: 0; }}
button:hover {{ background: #80B6FF; }}
.error {{ color: #EF4444; }}
.ok {{ color: #22C55E; }}
.nav {{ margin-bottom: 20px; }}
.nav a {{ margin-right: 16px; }}
</style></head><body>
<div class="nav">
  <a href="/">Главная</a>
  <a href="/topology">Топология</a>
  <a href="/flows">Flows</a>
  <a href="/matrices">Матрицы</a>
  <a href="/algorithms">Алгоритмы</a>
  <a href="/log">Лог</a>
  <a href="/reload">Reload</a>
</div>
{body}
</body></html>"""
        return Response(content_type="text/html; charset=utf-8",
                        body=page, status=200)

    # ----------------------------------------------------------------
    # Endpoints
    # ----------------------------------------------------------------

    @route("home", "/", methods=["GET"])
    def home(self, req, **kwargs):
        """Минималистичный главный экран контроллера.
        Управление маршрутизацией — целиком в Qt-приложении SDN Topology.
        Веб-страница оставлена как наблюдательная: текущее состояние и
        ссылки на лог запусков / статус. Раньше тут был список алгоритмов
        и формы — убрано по запросу пользователя как излишнее."""
        self._switches_reachability_formation()
        sw_count = len(self.controller.switches)
        body = f"""
<h1>SDN Topology — Ryu контроллер</h1>

<div class="card">
  <h2>Состояние</h2>
  <p>Подключено коммутаторов: <b>{sw_count}</b><br>
     Активный алгоритм маршрутизации:
       <b>{self.controller.active_algorithm or '— (обычный L2 learning)'}</b><br>
     Метрика: <b>{self.controller.active_metric}</b><br>
     Файл метрик: <code>{self.metrics_path}</code>
       ({'найден' if os.path.exists(self.metrics_path) else 'НЕ найден'})</p>
  <p style="margin-top:8px;">
    <a href="/log">Лог последнего запуска</a> |
    <a href="/topology">Топология (JSON)</a> |
    <a href="/route/status">Статус маршрутизации (JSON)</a>
  </p>
  <p style="color:#94A3B8; margin-top:12px;">Управление маршрутизацией
     и запуск алгоритмов — в приложении SDN Topology, меню
     <i>Алгоритмы → Запустить алгоритм…</i></p>
</div>
"""
        return self._html(body)

    @route("algos_list", "/algorithms/list", methods=["GET"])
    def algos_list(self, req, **kwargs):
        result = []
        for name, cls in sorted(self.algorithms.items()):
            result.append({
                "name": name,
                "description": cls.description,
                "params": cls.params,
                "is_user": bool(getattr(cls, "is_user", False)),
                "source": getattr(cls, "source_file", "algorithms.py"),
            })
        return self._json(200, {"algorithms": result})

    @route("topology", "/topology", methods=["GET"])
    def topology(self, req, **kwargs):
        self._switches_reachability_formation()
        graphs = self._build_graphs()
        body = f"""
<h1>Топология</h1>
<div class="card">
  <h3>Достижимость коммутаторов (порты)</h3>
  <pre>{json.dumps(self.controller.switches_reachability, indent=2, default=str)}</pre>
</div>
<div class="card">
  <h3>Граф смежности (метрика delay)</h3>
  <pre>{json.dumps(graphs.get("delay", {}), indent=2, default=str)}</pre>
</div>
<div class="card">
  <h3>Граф смежности (метрика loss)</h3>
  <pre>{json.dumps(graphs.get("loss", {}), indent=2, default=str)}</pre>
</div>
"""
        return self._html(body, "Топология")

    @route("log", "/log", methods=["GET"])
    def log(self, req, **kwargs):
        last = ControllerRest._last_run
        body = f"""
<h1>Последний запуск алгоритма</h1>
<div class="card">
  <h3>Алгоритм: {last.get('name') or '—'}</h3>
  <p>Параметры: <code>{last.get('params')}</code></p>
"""
        if last.get("error"):
            body += f'<p class="error">Ошибка: {last["error"]}</p>'
        if last.get("result"):
            body += f'<h4>Результат</h4><pre>{json.dumps(last["result"], indent=2, ensure_ascii=False, default=str)}</pre>'
        body += f'<h4>Лог</h4><pre>{last.get("log") or "(нет)"}</pre></div>'
        return self._html(body, "Лог")

    @route("reload", "/reload", methods=["GET"])
    def reload(self, req, **kwargs):
        before = set(self.algorithms.keys())
        self.algorithms = load_all_algorithms(self.user_dir)
        after = set(self.algorithms.keys())
        added = after - before
        removed = before - after
        body = f"""
<h1>Перезагрузка алгоритмов</h1>
<div class="card">
  <p>Всего: <b>{len(self.algorithms)}</b></p>
  <p class="ok">Добавлены: {', '.join(sorted(added)) or '—'}</p>
  <p class="error">Удалены: {', '.join(sorted(removed)) or '—'}</p>
</div>
"""
        return self._html(body, "Reload")

    @route("route_activate", "/route/activate", methods=["GET"])
    def route_activate(self, req, **kwargs):
        """Активирует алгоритм для маршрутизации трафика.
        Поддерживает оба варианта параметров:
          /route/activate?name=Дейкстра&metric=delay
          /route/activate?name=Dijkstra&metric=bandwidth
        После активации все НОВЫЕ IP-потоки будут идти по маршрутам,
        вычисленным указанным алгоритмом. Старые flow удаляются."""
        name = req.GET.get("name", "")
        metric = req.GET.get("metric", "delay")
        if name and name not in self.algorithms:
            return self._json(400, {"error": f"Алгоритм '{name}' не найден",
                                    "available": list(self.algorithms.keys())})
        self.controller.set_active_algorithm(name or None, metric)
        return self._json(200, {
            "ok": True,
            "active_algorithm": self.controller.active_algorithm,
            "metric": self.controller.active_metric,
            "switches_known": len(self.controller.switches),
        })

    @route("route_clear", "/route/clear", methods=["GET"])
    def route_clear(self, req, **kwargs):
        """Очищает активный алгоритм — возврат к обычному L2 learning."""
        self.controller.set_active_algorithm(None)
        return self._json(200, {"ok": True, "active_algorithm": None})

    @route("route_status", "/route/status", methods=["GET"])
    def route_status(self, req, **kwargs):
        return self._json(200, {
            "active_algorithm": self.controller.active_algorithm,
            "metric": self.controller.active_metric,
            "switches": list(self.controller.switches.keys()),
            "host_locations": {
                mac: {"dpid": loc[0], "port": loc[1]}
                for mac, loc in self.controller.host_locations.items()
            },
            "mac_to_port": {
                str(dpid): {mac: port for mac, port in table.items()}
                for dpid, table in self.controller.mac_to_port.items()
            },
        })

    @route("algorithm", "/algorithm", methods=["GET"])
    def run_algorithm(self, req, **kwargs):
        name = req.GET.get("name", "")
        if name not in self.algorithms:
            return self._html(f'<h1>Алгоритм "{name}" не найден</h1>'
                              '<p><a href="/">← на главную</a></p>')

        self._switches_reachability_formation()
        algo_cls = self.algorithms[name]
        algo = algo_cls()

        # Собираем параметры из query
        params = {}
        for pname, meta in algo_cls.params.items():
            ptype = meta[0] if isinstance(meta, tuple) else "str"
            default = meta[1] if isinstance(meta, tuple) and len(meta) > 1 else None
            raw = req.GET.get(pname)
            if raw is None:
                params[pname] = default
                continue
            try:
                if ptype == "int":
                    params[pname] = int(raw)
                elif ptype == "float":
                    params[pname] = float(raw)
                else:
                    params[pname] = raw
            except ValueError:
                params[pname] = default

        # src/dst — отдельно
        try:
            src = int(req.GET.get("src", "1"))
            dst = int(req.GET.get("dst", "2"))
        except ValueError:
            return self._html('<h1 class="error">src и dst должны быть числами</h1>')

        graphs = self._build_graphs()

        ControllerRest._last_run["name"] = name
        ControllerRest._last_run["params"] = dict(params, src=src, dst=dst)

        try:
            result = algo.run(src, dst, graphs, params)
        except Exception:
            err = traceback.format_exc()
            ControllerRest._last_run["error"] = err
            ControllerRest._last_run["log"] = algo.get_log()
            ControllerRest._last_run["result"] = None
            return self._html(f'<h1 class="error">Ошибка в алгоритме {name}</h1>'
                              f'<pre>{err}</pre>'
                              f'<h3>Лог</h3><pre>{algo.get_log()}</pre>')

        ControllerRest._last_run["error"] = None
        ControllerRest._last_run["result"] = result
        ControllerRest._last_run["log"] = algo.get_log()

        # Отправляем визуализацию в SDN Topology через TCP
        try:
            if "routes" in result and result["routes"]:
                sdn_topology.visualize_routs(result["routes"])
            if "tree" in result and result["tree"]:
                sdn_topology.visualize_tree(result["tree"])
            if "segments" in result and result["segments"]:
                sdn_topology.visualize_segments(result["segments"])
        except Exception:
            pass

        body = f"""
<h1>Алгоритм: {name}</h1>
<div class="card">
  <p>Параметры: <code>src={src}, dst={dst}, {params}</code></p>
  <h3 class="ok">Результат</h3>
  <pre>{json.dumps(result, indent=2, ensure_ascii=False, default=str)}</pre>
  <h3>Лог</h3>
  <pre>{algo.get_log() or '(нет)'}</pre>
  <p><a href="/algorithms">← к списку алгоритмов</a></p>
</div>
"""
        return self._html(body, name)

    # ----------------------------------------------------------------
    # Веб-пульт: Flows / Матрицы / Алгоритмы (#5)
    # ----------------------------------------------------------------

    def _post_body(self, req):
        """Безопасно разбирает JSON-тело POST-запроса."""
        try:
            raw = req.body
            if isinstance(raw, bytes):
                raw = raw.decode("utf-8")
            return json.loads(raw) if raw else {}
        except Exception:
            return {}

    def _flow_humanize(self, r):
        """Возвращает (тип, условие, действие) в человекочитаемом виде."""
        cookie = (r.get("cookie") or "").lower()
        prio = r.get("priority", 0)
        match = r.get("match", {}) or {}
        # Тип правила
        if cookie == "0xa160c0de":
            kind = "🔵 алгоритм"
        elif prio == 0:
            kind = "table-miss"
        elif prio == 1:
            kind = "L2 learning"
        else:
            kind = "правило"
        # Условие (что матчим) — берём самое значимое
        if match.get("eth_dst") and match.get("eth_src"):
            cond = "%s → %s" % (match["eth_src"], match["eth_dst"])
        elif match.get("eth_dst"):
            cond = "кому %s" % match["eth_dst"]
        elif match.get("in_port"):
            cond = "с порта %s" % match["in_port"]
        elif not match:
            cond = "любой пакет"
        else:
            cond = ", ".join("%s=%s" % (k, v) for k, v in match.items())
        # Действие
        acts = r.get("actions", []) or []
        if not acts:
            act = "отбросить"
        elif any("CONTROLLER" in a or "controller" in a for a in acts):
            act = "→ контроллер"
        else:
            act = ", ".join(a.replace("output:", "→ порт ") for a in acts)
        return kind, cond, act

    @route("flows", "/flows", methods=["GET"])
    def flows(self, req, **kwargs):
        """Таблицы потоков по каждому свитчу — кратко и понятно."""
        self.controller.request_flow_stats()
        hub.sleep(0.6)  # ждём асинхронные ответы свитчей
        stats = self.controller.flow_stats
        total = sum(len(v) for v in stats.values())
        algo_total = sum(1 for v in stats.values() for r in v
                         if (r.get("cookie") or "").lower() == "0xa160c0de")
        body = ["<h1>Flow-таблицы коммутаторов</h1>",
                '<div class="card">'
                '<p>Всего правил: <b>%d</b>, из них от алгоритма: '
                '<b>%d</b> (помечены 🔵). Остальное — стандартный L2 learning '
                'и table-miss.</p>'
                '<p><a href="/flows">↻ обновить</a> &nbsp; '
                '<a href="/flows/clear"><button>Сбросить правила алгоритма</button></a>'
                '</p></div>' % (total, algo_total)]
        if not stats:
            body.append('<div class="card"><p>Пока нет данных. Коммутаторов '
                        'подключено: <b>%d</b>. Запустите топологию и дайте '
                        'трафик (pingall).</p></div>' % len(self.controller.switches))
        for dpid in sorted(stats.keys()):
            rows = stats[dpid]
            body.append('<div class="card"><h3>s%d — %d правил</h3>'
                        '<table><tr><th>Тип</th><th>Условие</th>'
                        '<th>Действие</th><th>Пакетов</th></tr>' % (dpid, len(rows)))
            for r in rows:
                kind, cond, act = self._flow_humanize(r)
                hl = ' style="color:#5BA0FF;font-weight:600;"' \
                     if "алгоритм" in kind else ""
                body.append("<tr><td%s>%s</td><td>%s</td><td>%s</td>"
                            "<td>%s</td></tr>" % (hl, kind, cond, act,
                                                  r.get("packet_count", 0)))
            body.append("</table></div>")
        return self._html("\n".join(body), "Flows")

    @route("flows_clear", "/flows/clear", methods=["GET"])
    def flows_clear(self, req, **kwargs):
        """Снимает только flow-правила алгоритма (по cookie) — L2 не трогаем."""
        self.controller._clear_algorithm_flows()
        body = ('<h1>Flows очищены</h1><div class="card">'
                '<p class="ok">Правила активной маршрутизации сняты. '
                'Трафик идёт по обычному L2 learning.</p>'
                '<p><a href="/flows">← к Flow-таблицам</a></p></div>')
        return self._html(body, "Flows")

    def _dijkstra(self, graph, src):
        """Кратчайшие расстояния от src по словарю смежности graph."""
        import heapq
        dist = {src: 0}
        pq = [(0, src)]
        while pq:
            d, u = heapq.heappop(pq)
            if d > dist.get(u, float("inf")):
                continue
            for v, w in graph.get(u, {}).items():
                nd = d + w
                if nd < dist.get(v, float("inf")):
                    dist[v] = nd
                    heapq.heappush(pq, (nd, v))
        return dist

    def _matrix_html(self, title, nodes, cell):
        rows = ['<div class="card"><h3>%s</h3><table><tr><th>&nbsp;</th>' % title]
        rows += ["<th>s%d</th>" % n for n in nodes]
        rows.append("</tr>")
        for a in nodes:
            rows.append("<tr><th>s%d</th>" % a)
            for b in nodes:
                rows.append("<td>%s</td>" % cell(a, b))
            rows.append("</tr>")
        rows.append("</table></div>")
        return "".join(rows)

    @route("matrices", "/matrices", methods=["GET"])
    def matrices(self, req, **kwargs):
        self._switches_reachability_formation()
        reach = self.controller.switches_reachability
        nodes = sorted(self.controller.switches.keys())
        graphs = self._build_graphs()

        body = ["<h1>Матрицы</h1>"]

        # 1. Матрица портов (s_a → s_b через какой порт).
        body.append(self._matrix_html(
            "Порты соседства (s_a → s_b)", nodes,
            lambda a, b: reach.get(a, {}).get(b, "·")))

        # 2. Матрица достижимости (по графу delay, BFS-расстояние в хопах).
        adj_hops = {a: {b: 1 for b in reach.get(a, {})} for a in nodes}
        def reach_cell(a, b):
            if a == b:
                return "0"
            d = self._dijkstra(adj_hops, a).get(b)
            return str(d) if d is not None else "∞"
        body.append(self._matrix_html("Достижимость (хопов)", nodes, reach_cell))

        # 3. Матрица расстояний по метрике delay.
        gdelay = graphs.get("delay", {})
        def dist_cell(a, b):
            if a == b:
                return "0"
            d = self._dijkstra(gdelay, a).get(b)
            return ("%g" % d) if d is not None else "∞"
        body.append(self._matrix_html("Расстояния (метрика delay)", nodes, dist_cell))

        # 4. Хост → свитч (где подключён каждый хост).
        hs = ['<div class="card"><h3>Хост → коммутатор</h3>'
              '<table><tr><th>MAC хоста</th><th>Коммутатор</th><th>Порт</th></tr>']
        for mac, loc in sorted(self.controller.host_locations.items()):
            hs.append("<tr><td>%s</td><td>s%d</td><td>%d</td></tr>"
                      % (mac, loc[0], loc[1]))
        if not self.controller.host_locations:
            hs.append('<tr><td colspan="3">— хостов пока не видно '
                      '(нужен трафик/ARP) —</td></tr>')
        hs.append("</table></div>")
        body.append("".join(hs))

        return self._html("\n".join(body), "Матрицы")

    @route("algorithms_page", "/algorithms", methods=["GET"])
    def algorithms_page(self, req, **kwargs):
        nodes = sorted(self.controller.switches.keys())
        s_def = nodes[0] if nodes else 1
        d_def = nodes[-1] if len(nodes) > 1 else (nodes[0] if nodes else 2)
        body = ['<h1>Алгоритмы маршрутизации</h1>',
                '<div class="card"><p>Активный: <b>%s</b> (метрика %s). '
                'Запуск ниже считает путь src→dst и шлёт визуализацию в '
                'приложение SDN Topology.</p></div>'
                % (self.controller.active_algorithm or "— (L2 learning)",
                   self.controller.active_metric)]
        body.append('<div class="card"><table>'
                    '<tr><th>Алгоритм</th><th>Описание</th>'
                    '<th>Источник</th><th>Запуск</th></tr>')
        for name, cls in sorted(self.algorithms.items()):
            src_lbl = "пользовательский" if getattr(cls, "is_user", False) else "встроенный"
            run = ('<a href="/algorithm?name=%s&src=%s&dst=%s">'
                   '<button>s%s→s%s</button></a>'
                   % (name, s_def, d_def, s_def, d_def))
            body.append("<tr><td><b>%s</b></td><td>%s</td><td>%s</td><td>%s</td></tr>"
                        % (name, getattr(cls, "description", ""), src_lbl, run))
        body.append("</table></div>")
        return self._html("\n".join(body), "Алгоритмы")
