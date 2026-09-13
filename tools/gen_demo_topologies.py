#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Генератор двух демонстрационных топологий SDN Topology (по ~30 узлов).

Создаёт открываемые проекты (topology.sdn.xml + .snet/project.json +
metrics.txt + копия ryu-шаблонов):
  • sdn-demo-mesh — сетка 4x4 + «экспресс»-диагонали + каналы с потерями.
    Для: Дейкстра (delay vs bandwidth), LARAC (учёт потерь), отказ канала.
  • sdn-demo-ring — 5 кластеров по кольцу.
    Для: Жадное сегментирование, Парные переходы, кольцевая отказоустойчивость.

Имена узлов назначаются редактором по порядку: h1.., d1.., s1.., c0.
Поэтому элементы в каждой секции выводятся строго в порядке нумерации.
"""
import os
import math
import shutil
import argparse

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RYU_SRC = os.path.join(REPO, "resources", "templates", "ryu")

# ------- пресеты Docker (host-порты должны быть уникальны в пределах топологии) -------
def docker_nginx(host_port):
    return dict(image="nginx", command="/docker-entrypoint.sh nginx -g 'daemon off;'",
                env=[], ports=["%d:80" % host_port])
def docker_postgres(host_port):
    return dict(image="postgres:16", command="docker-entrypoint.sh postgres",
                env=["POSTGRES_PASSWORD=secret", "POSTGRES_DB=demo", "POSTGRES_USER=demo"],
                ports=["%d:5432" % host_port])
def docker_mysql(host_port):
    return dict(image="mysql:8", command="docker-entrypoint.sh mysqld",
                env=["MYSQL_ROOT_PASSWORD=secret", "MYSQL_DATABASE=demo"],
                ports=["%d:3306" % host_port])
def docker_python(host_port):
    return dict(image="python:3.11-slim", command="python3 -m http.server 8000",
                env=[], ports=["%d:8000" % host_port])


class Topo:
    def __init__(self):
        self.hosts = []      # [(x,y)]            -> h1, h2, ...
        self.dockers = []    # [dict + x,y]       -> d1, d2, ...
        self.switches = []   # [(x,y)]            -> s1, s2, ...
        self.sslinks = []    # [(n1,n2,delay,bw,loss)]
        self.ctrl = (80, 80)

    def add_host(self, x, y):
        self.hosts.append((x, y)); return "h%d" % len(self.hosts)

    def add_docker(self, x, y, preset):
        ip = "10.0.0.%d" % (100 + len(self.dockers) + 1)
        mac = "00:00:00:00:0d:%02d" % (len(self.dockers) + 1)
        d = dict(preset); d.update(x=x, y=y, ip=ip, mac=mac)
        self.dockers.append(d); return "d%d" % len(self.dockers)

    def add_switch(self, x, y):
        self.switches.append((x, y)); return "s%d" % len(self.switches)

    def link(self, n1, n2, delay, bw=100, loss=0):
        # Дедуп пары (в любом направлении): дубликат ломал назначение портов
        # (порт -1 → топология не собиралась в Mininet) и терял метрики.
        # Оставляем ПОСЛЕДНЕЕ определение — так «грязный» канал переопределяет
        # обычное ребро сетки.
        self.sslinks = [e for e in self.sslinks
                        if not ((e[0], e[1]) == (n1, n2) or (e[0], e[1]) == (n2, n1))]
        self.sslinks.append((n1, n2, delay, bw, loss))

    # ---- сериализация ----
    def xml(self):
        L = ['<?xml version="1.0" encoding="utf-8"?>', "<!DOCTYPE topology>", "<Network>"]
        L.append(" <Hosts>")
        for (x, y) in self.hosts:
            # ip/mac хостов редактор назначает сам по индексу — пишем для совместимости
            i = self.hosts.index((x, y))  # не используется для имени, только заглушка
            L.append('  <Host mac="00:00:00:00:00:00" y="%d" x="%d" ip="10.0.0.0"/>' % (y, x))
        L.append(" </Hosts>")
        L.append(" <Dockers>")
        for d in self.dockers:
            L.append('  <Docker command="%s" mac="%s" y="%d" x="%d" image="%s" ip="%s">'
                     % (_esc(d["command"]), d["mac"], d["y"], d["x"], _esc(d["image"]), d["ip"]))
            for e in d["env"]:
                L.append("   <Env>%s</Env>" % _esc(e))
            for p in d["ports"]:
                L.append("   <Port>%s</Port>" % _esc(p))
            L.append("  </Docker>")
        L.append(" </Dockers>")
        L.append(" <Switches>")
        for (x, y) in self.switches:
            L.append('  <Switch y="%d" x="%d"/>' % (y, x))
        L.append(" </Switches>")
        L.append(" <Controllers>")
        L.append('  <Controller y="%d" x="%d" ip="127.0.0.1" port="6653"/>' % (self.ctrl[1], self.ctrl[0]))
        L.append(" </Controllers>")
        L.append(" <SSLinks>")
        for (n1, n2, delay, bw, loss) in self.sslinks:
            L.append('  <Link disabled="false" bandwidth="%d" delay="%d" node2="%s" loss="%d" node1="%s"/>'
                     % (bw, delay, n2, loss, n1))
        L.append(" </SSLinks>")
        L.append(" <CSLinks>")
        for i in range(len(self.switches)):
            L.append('  <Link node2="s%d" node1="c0"/>' % (i + 1))
        L.append(" </CSLinks>")
        L.append(" <Texts/>")
        L.append("</Network>")
        return "\n".join(L) + "\n"

    def metrics(self):
        """metrics.txt: i-j:delay,bw,loss для каналов между свитчами (обе стороны)."""
        out = []
        for (n1, n2, delay, bw, loss) in self.sslinks:
            if n1.startswith("s") and n2.startswith("s"):
                a = n1[1:]; b = n2[1:]
                out.append("%s-%s:%d,%d,%d" % (a, b, delay, bw, loss))
                out.append("%s-%s:%d,%d,%d" % (b, a, delay, bw, loss))
        return "\n".join(out) + "\n"


def _esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
             .replace('"', "&quot;"))


# =====================================================================
# Топология 1: MESH-GRID 4x4
# =====================================================================
def build_mesh():
    t = Topo()
    t.ctrl = (60, 60)
    # 16 свитчей сеткой 4x4 (s1..s16, нумерация по строкам)
    pos = {}
    for r in range(4):
        for c in range(4):
            idx = r * 4 + c + 1
            x = 320 + c * 210
            y = 200 + r * 170
            t.add_switch(x, y)
            pos[idx] = (x, y)

    def s(i):  # имя свитча
        return "s%d" % i

    # Рёбра сетки (соседи по горизонтали/вертикали) с разными delay.
    grid_delay = {}
    base = [
        # горизонтальные (рёбра 6-7 и 10-11 заданы ниже как «грязные»)
        (1,2,9),(2,3,8),(3,4,10),
        (5,6,7),(7,8,8),
        (9,10,9),(11,12,11),
        (13,14,8),(14,15,10),(15,16,9),
        # вертикальные
        (1,5,8),(5,9,9),(9,13,7),
        (2,6,10),(6,10,8),(10,14,9),
        (3,7,7),(7,11,11),(11,15,8),
        (4,8,9),(8,12,8),(12,16,10),
    ]
    for a, b, d in base:
        t.link(s(a), s(b), d, bw=100, loss=0)

    # «Экспресс»-диагонали: маленький delay, но НИЗКАЯ полоса (bw=20) →
    # по delay Дейкстра их выберет, по bandwidth — обойдёт (контраст метрик).
    for a, b in [(1,6),(6,11),(11,16),(4,7),(7,10),(10,13)]:
        t.link(s(a), s(b), 3, bw=20, loss=0)

    # Каналы с высокими потерями (low delay) — для LARAC: быстрый, но «грязный».
    t.link(s(6), s(7), 4, bw=100, loss=35)
    t.link(s(10), s(11), 4, bw=100, loss=30)
    t.link(s(2), s(5), 3, bw=100, loss=40)

    # 10 хостов по периметру (h1..h10)
    host_attach = [1, 4, 13, 16, 2, 8, 9, 15, 5, 12]
    hpos = {1:(170,150),4:(1120,150),13:(170,820),16:(1120,820),
            2:(560,70),8:(1180,540),9:(140,540),15:(760,900),
            5:(140,360),12:(1180,710)}
    for sw in host_attach:
        x, y = hpos[sw]
        h = t.add_host(x, y)
        t.link(h, s(sw), 1, bw=100, loss=0)

    # 4 докера на внутренних свитчах (d1..d4)
    dock = [
        (6,  docker_postgres(15432), (560, 300)),
        (11, docker_nginx(8088),     (760, 540)),
        (7,  docker_mysql(13306),    (760, 300)),
        (10, docker_python(8000),    (560, 540)),
    ]
    for sw, preset, (x, y) in dock:
        dn = t.add_docker(x, y, preset)
        t.link(dn, s(sw), 1, bw=100, loss=0)
    return t


# =====================================================================
# Топология 2: RING-OF-CLUSTERS (5 кластеров по кольцу)
# =====================================================================
def build_ring():
    t = Topo()
    t.ctrl = (60, 60)
    cx, cy, R = 700, 470, 330
    rT = 95  # радиус треугольника кластера

    gateways = []   # имя gateway-свитча каждого кластера
    cluster_sws = []  # списки имён свитчей по кластерам
    cluster_centers = []
    for k in range(5):
        ang = math.radians(90 - k * 72)
        ccx = cx + R * math.cos(ang)
        ccy = cy - R * math.sin(ang)
        cluster_centers.append((ccx, ccy))
        sws = []
        for j in range(3):  # треугольник из 3 свитчей
            a2 = math.radians(90 - k * 72 + (j - 1) * 40)
            sx = int(ccx + rT * math.cos(a2))
            sy = int(ccy - rT * math.sin(a2))
            sws.append(t.add_switch(sx, sy))
        cluster_sws.append(sws)
        gateways.append(sws[0])  # первый свитч кластера — gateway
        # плотные дешёвые внутрикластерные связи (треугольник)
        t.link(sws[0], sws[1], 1, bw=100, loss=0)
        t.link(sws[1], sws[2], 2, bw=100, loss=0)
        t.link(sws[2], sws[0], 1, bw=100, loss=0)

    # Кольцо: gateway[i] — gateway[i+1], ДОРОГИЕ каналы (delay 20) →
    # сегментирование режет именно их, выделяя 5 кластеров.
    ring_delays = [20, 22, 18, 21, 19]
    for k in range(5):
        t.link(gateways[k], gateways[(k + 1) % 5], ring_delays[k], bw=100, loss=0)

    # 2 хоста на кластер (на не-gateway свитчи) → h1..h10
    for k in range(5):
        ccx, ccy = cluster_centers[k]
        for j in (1, 2):
            ang = math.radians(90 - k * 72 + (j - 1) * 40)
            hx = int(ccx + (rT + 70) * math.cos(ang))
            hy = int(ccy - (rT + 70) * math.sin(ang))
            h = t.add_host(hx, hy)
            t.link(h, cluster_sws[k][j], 1, bw=100, loss=0)

    # 1 докер на кластер → d1..d5 (host-порты уникальны!)
    presets = [docker_nginx(8088), docker_postgres(15432), docker_mysql(13306),
               docker_python(8000), docker_nginx(8089)]
    for k in range(5):
        ccx, ccy = cluster_centers[k]
        dx = int(ccx); dy = int(ccy)
        dn = t.add_docker(dx, dy, presets[k])
        t.link(dn, cluster_sws[k][0], 1, bw=100, loss=0)  # докер на gateway
    return t


# =====================================================================
# Топология 3: CORE/EDGE BACKBONE (двухуровневая магистраль, 34 узла)
# =====================================================================
def build_core():
    """Ядро из 4 магистральных свитчей (полный меш) + 12 edge-свитчей
    (по 3 на ядро) + быстрый узкий шорткат + «грязные» шорткаты. Большая
    топология с избыточностью — хороша для отказа каналов и LARAC."""
    t = Topo()
    t.ctrl = (60, 60)
    cx, cy = 800, 520

    # --- ядро: 4 магистральных свитча квадратом, ПОЛНЫЙ меш (s1..s4) ---
    core_pos = [(640, 360), (960, 360), (960, 680), (640, 680)]
    core = [t.add_switch(x, y) for (x, y) in core_pos]
    # Кольцо ядра — дёшево (delay 5), диагонали — дорого (delay 13):
    # при отказе кольцевого ребра трафик идёт по диагонали (запас прочности).
    t.link(core[0], core[1], 5, bw=1000)
    t.link(core[1], core[2], 5, bw=1000)
    t.link(core[2], core[3], 5, bw=1000)
    t.link(core[3], core[0], 5, bw=1000)
    t.link(core[0], core[2], 13, bw=1000)
    t.link(core[1], core[3], 13, bw=1000)

    # --- агрегация: по 3 edge-свитча на каждое ядро (s5..s16) ---
    edge_uplink_delay = [2, 3, 4]
    edges = []          # edges[core_index] = [имена 3 edge-свитчей]
    edge_pos = {}       # имя -> (x,y)
    for ci, (cxp, cyp) in enumerate(core_pos):
        base_ang = math.atan2(cyp - cy, cxp - cx)   # наружу от центра
        cl = []
        for j in range(3):
            a = base_ang + (j - 1) * 0.55
            ex = int(cxp + 230 * math.cos(a))
            ey = int(cyp + 230 * math.sin(a))
            name = t.add_switch(ex, ey)
            edge_pos[name] = (ex, ey)
            cl.append(name)
            t.link(core[ci], name, edge_uplink_delay[j], bw=100)
        edges.append(cl)

    # --- спец-каналы для демонстраций ---
    # Быстрый, но УЗКИЙ магистральный шорткат через всю ткань (delay vs bw):
    t.link(edges[0][0], edges[2][0], 2, bw=20, loss=0)    # s5 — s11
    # «Грязные» быстрые шорткаты между edge-свитчами (для LARAC):
    t.link(edges[0][1], edges[2][1], 3, bw=100, loss=30)  # s6 — s12
    t.link(edges[1][1], edges[3][1], 3, bw=100, loss=40)  # s9 — s15
    t.link(edges[1][2], edges[3][2], 3, bw=100, loss=25)  # s10 — s16

    # --- 12 хостов: по одному на каждый edge-свитч (h1..h12) ---
    flat = [n for cl in edges for n in cl]
    for name in flat:
        ex, ey = edge_pos[name]
        ang = math.atan2(ey - cy, ex - cx)
        hx = int(ex + 95 * math.cos(ang))
        hy = int(ey + 95 * math.sin(ang))
        h = t.add_host(hx, hy)
        t.link(h, name, 1, bw=100, loss=0)

    # --- 6 докеров на части edge-свитчей (d1..d6), порты уникальны ---
    dock_presets = [docker_nginx(8090), docker_postgres(15433), docker_mysql(13307),
                    docker_python(8001), docker_nginx(8091), docker_postgres(15434)]
    dock_on = [flat[0], flat[2], flat[4], flat[6], flat[8], flat[10]]
    for name, preset in zip(dock_on, dock_presets):
        ex, ey = edge_pos[name]
        ang = math.atan2(ey - cy, ex - cx) + 0.4
        dx = int(ex + 95 * math.cos(ang))
        dy = int(ey + 95 * math.sin(ang))
        dn = t.add_docker(dx, dy, preset)
        t.link(dn, name, 1, bw=100, loss=0)
    return t


# =====================================================================
# README-тексты (пишутся рядом с топологией)
# =====================================================================
README_MESH = """# sdn-demo-mesh — сетка 4×4 (30 узлов)

**Состав:** 16 коммутаторов сеткой 4×4, 10 хостов по периметру, 4 докера
(postgres d1, nginx d2, mysql d3, python d4), контроллер c0.

**Особенности метрик:**
- Обычные рёбра сетки: `delay 7–12`, `bw 100`, `loss 0`.
- «Экспресс»-диагонали `s1–s6–s11–s16` и `s4–s7–s10–s13`: `delay 3`, но
  `bw 20` (узкие).
- Каналы с потерями: `s6–s7 loss 35`, `s10–s11 loss 30`, `s2–s5 loss 40`
  (быстрые, но «грязные»).

## Что демонстрировать

### Дейкстра — метрика delay vs bandwidth (главный контраст)
- Алгоритмы → Дейкстра, метрика **delay** → путь идёт через экспресс-диагонали
  (delay 3).
- Тот же запуск с метрикой **bandwidth** → путь **обходит** диагонали (у них
  bw 20), идёт по широким рёбрам сетки. Наглядно: одна топология — разные
  маршруты от выбора метрики.

### LARAC (QoS — быстро, но без потерь) — главный demo
Параметры в диалоге «Алгоритмы»:
- **Алгоритм:** LARAC
- **Метрика (1-я):** Задержка (delay) — её минимизируем
- **Вторая метрика (ограничение):** Потери пакетов (loss)
- **Ограничение R₂:** `0` — суммарные потери на маршруте = 0
- **Коэффициент λ:** `0` (авто — множитель подбирается сам)

Что видно: LARAC строит самый быстрый маршрут, **полностью обходя** грязные
каналы `s6–s7 (loss35)`, `s10–s11 (loss30)`, `s2–s5 (loss40)`, даже если они
короче по delay. Сравните с **Дейкстрой по delay** — та спокойно идёт через них.

Варианты для наглядности:
- **R₂ = 0** — ни одного потерянного пакета (объезжает все loss-каналы).
- **R₂ = 40** — допускаем до 40 ед. потерь: LARAC уже готов пройти через один
  грязный, но быстрый канал, если это заметно укорачивает delay.
- **λ = 1.5** (ручной режим, R₂ игнорируется) — один проход по `delay + 1.5·loss`:
  видно, как множитель Лагранжа «отталкивает» от грязных рёбер.

После запуска маршруты по умолчанию строятся от s1. ПКМ по любому свичу →
«Подсветить маршруты от этого свича» — добавит его дерево своим цветом.

### Отказ канала
- ПКМ по экспресс-каналу (например `s6–s11`) → «Отключить канал». Маршрут
  мгновенно перестраивается в обход (через рёбра сетки) — и на холсте, и в Ryu
  (flow пересчитываются по EventLinkDelete). Включите обратно — вернётся.
"""

README_RING = """# sdn-demo-ring — 5 кластеров по кольцу (30 узлов)

**Состав:** 15 коммутаторов = 5 кластеров по 3 свитча (треугольник),
10 хостов (по 2 на кластер), 5 докеров (по 1 на кластер: nginx/postgres/
mysql/python/nginx), контроллер c0.

**Особенности метрик (community-структура):**
- Внутри кластера: плотные дешёвые связи `delay 1–2`.
- Между кластерами (кольцо gateway↔gateway `s1–s4–s7–s10–s13–s1`):
  дорогие связи `delay 18–22`.

Такая структура «дёшево внутри, дорого между» — идеальна для сегментирования
и для демонстрации обходных путей по кольцу.

## Что демонстрировать

### Жадное сегментирование (главная фишка этой топологии)
- Алгоритмы → Жадное сегментирование, **K = 5**. Алгоритм режет дорогие
  кольцевые каналы и выделяет ровно 5 кластеров — каждый своим цветом
  (цветные кольца на узлах + межсегментные пунктирные «мосты»). Меняйте K
  (3, 4, 5) — видно, как меняется разбиение.

### Парные переходы (backup-пути)
- Алгоритмы → Парные переходы. На кольце для каждой пары gateway есть
  основной и резервный путь (по кольцу в обе стороны). Наглядно показывает
  отказоустойчивость маршрутизации.

### Отказ канала на кольце (самая зрелищная демонстрация)
- Запустите топологию, дайте трафик между кластерами (ping). ПКМ по кольцевому
  каналу (например `s1–s4`) → «Отключить канал». Трафик между кластерами
  пойдёт **в обход по кольцу** (длинный путь через остальные gateway).
  И на холсте, и в Ryu маршрут перестроится. Включите обратно — вернётся
  короткий путь.

### Дейкстра
- Тоже работает: путь между хостами разных кластеров идёт через минимум
  дорогих кольцевых рёбер.
"""

README_CORE = """# sdn-demo-core — двухуровневая магистраль core/edge (34 узла)

**Состав:** 16 коммутаторов = 4 магистральных (ядро, полный меш) + 12
агрегирующих (edge, по 3 на каждое ядро), 12 хостов (по 1 на edge),
6 докеров, контроллер c0.

**Структура и метрики:**
- **Ядро** `s1–s4` — полный меш, `bw 1000`. Кольцо ядра дешёвое (`delay 5`),
  диагонали `s1–s3` и `s2–s4` дорогие (`delay 13`) — резерв на отказ.
- **Uplink edge→ядро:** `bw 100`, `delay 2–4`.
- **Узкий экспресс** `s5–s11`: `delay 2`, но `bw 20` (через всю ткань).
- **«Грязные» шорткаты:** `s6–s12 loss 30`, `s9–s15 loss 40`, `s10–s16 loss 25`
  (`bw 100`, `delay 3`) — быстрые межрайонные перемычки в обход ядра.

## Что демонстрировать

### Отказ канала на ядре (главная фишка этой топологии)
- Запустите топологию, дайте трафик между хостами разных районов. ПКМ по
  кольцевому ребру ядра (например `s1–s2`) → «Отключить канал». Маршрут уходит
  на **диагональ ядра** (`s1–s3`/`s2–s4`) или по длинному кольцу — и на холсте,
  и в Ryu. Полный меш ядра делает сеть отказоустойчивой.

### LARAC (быстро, но без потерь)
- LARAC, метрика1 = **delay**, метрика2 = **loss**, **R₂ = 0**, **λ = 0**.
- Между районами есть соблазнительные быстрые перемычки `s6–s12 (loss30)`,
  `s9–s15 (loss40)`, `s10–s16 (loss25)`. LARAC их **обходит** (через ядро),
  Дейкстра по delay — использует. Поднимите **R₂ = 30** — LARAC согласится
  пройти через `s10–s16` (loss 25 ≤ 30).

### Дейкстра — delay vs bandwidth
- По **delay** путь между s5- и s11-районами идёт по узкому экспрессу `s5–s11`
  (delay 2). По **bandwidth** — обходит его (bw 20) через ядро (bw 1000).

### Жадное сегментирование
- **K = 4** — алгоритм выделяет 4 района вокруг магистральных свитчей,
  разрезая дорогие/межрайонные связи.

После запуска маршруты строятся от s1. ПКМ по свичу → «Подсветить маршруты от
этого свича» — дерево этого свича своим цветом (наложение: побеждает последний).
"""


def write_project(path, name, topo, readme=None):
    os.makedirs(os.path.join(path, ".snet"))
    with open(os.path.join(path, "topology.sdn.xml"), "w", encoding="utf-8") as f:
        f.write(topo.xml())
    with open(os.path.join(path, "metrics.txt"), "w", encoding="utf-8") as f:
        f.write(topo.metrics())
    with open(os.path.join(path, ".snet", "project.json"), "w", encoding="utf-8") as f:
        f.write('{\n    "name": "%s",\n    "version": 1,\n    "editor": "SDN Topology (SNet)"\n}\n' % name)
    if readme:
        with open(os.path.join(path, "README.md"), "w", encoding="utf-8") as f:
            f.write(readme)
    # копируем ryu-шаблоны
    shutil.copytree(RYU_SRC, os.path.join(path, "ryu"))
    n = len(topo.hosts) + len(topo.dockers) + len(topo.switches)
    print("  %s: %d узлов (%d свитчей, %d хостов, %d докеров), %d каналов"
          % (name, n, len(topo.switches), len(topo.hosts), len(topo.dockers), len(topo.sslinks)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate demo projects in a new directory.")
    parser.add_argument("--output", default=os.path.join(REPO, "demo-projects"))
    args = parser.parse_args()
    output = os.path.abspath(args.output)
    if os.path.exists(output):
        parser.error("Output directory already exists; choose a new --output path.")
    os.makedirs(output)
    print("Генерация демо-топологий:")
    write_project(os.path.join(output, "sdn-demo-mesh"), "sdn-demo-mesh", build_mesh(), README_MESH)
    write_project(os.path.join(output, "sdn-demo-ring"), "sdn-demo-ring", build_ring(), README_RING)
    write_project(os.path.join(output, "sdn-demo-core"), "sdn-demo-core", build_core(), README_CORE)
    print("Готово. Открывайте через «Файл > Открыть проект» в SDN Topology.")
