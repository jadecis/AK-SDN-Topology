"""
SNet — обёртка для отправки результатов алгоритмов в визуальную среду.

Визуальная среда (SDN Topology) держит TCP-сервер на 127.0.0.1:6111
и принимает короткие сообщения:

    Trees#1-2,2-3,3-7,            — рёбра остовного дерева
    Paths#1,2,3,7;1,4,5,7;        — список маршрутов (точкой с запятой)
    Islands#1,2,3;4,5;            — список сегментов сети

После получения такого сообщения визуальная среда подсвечивает
соответствующие элементы на холсте.
"""

import re
import socket


def _send_message(message):
    """Открывает соединение, отправляет байтовое сообщение, закрывает.
    В случае ошибки печатает её и продолжает (не валит контроллер)."""
    try:
        s = socket.socket()
        s.settimeout(2.0)
        s.connect(("127.0.0.1", 6111))
        s.send(message if isinstance(message, bytes) else message.encode("utf-8"))
        s.close()
    except Exception as e:
        import sys
        sys.stderr.write("[sdn_topology] " + str(e) + "\n")


def visualize_tree(edges):
    """edges = [[1,2],[2,3],...]"""
    parts = []
    for e in edges:
        if len(e) >= 2:
            parts.append(f"{e[0]}-{e[1]}")
    msg = "Trees#" + ",".join(parts) + ","
    _send_message(msg)


def visualize_routs(routs):
    """routs = [[1,2,3,7], [1,4,5,7], ...]"""
    msg = "Paths#"
    for path in routs:
        if not path:
            continue
        msg += ",".join(str(n) for n in path) + ";"
    _send_message(msg)


def visualize_segments(segments):
    """segments = [[1,2,3],[4,5],...]"""
    msg = "Islands#"
    for seg in segments:
        if not seg:
            continue
        msg += ",".join(str(n) for n in seg) + ";"
    _send_message(msg)


def clear_marks():
    """Снять подсветку (пока — отправка пустого Paths#)."""
    _send_message("Paths#")


# ----------------------------------------------------------------------
# Чтение metrics.txt
# ----------------------------------------------------------------------

def read_matrix(size, metric_data_file):
    """Читает metrics.txt и возвращает три словаря смежности:
       {"delay": {1: {2: 5, ...}, ...},
        "bandwidth": {...},
        "loss": {...}}

    Файл имеет строки вида:  1-2:5,100,0
    (sw1-sw2:delay,bandwidth,loss)
    """
    matrix = {"delay": {}, "bandwidth": {}, "loss": {}}
    try:
        with open(metric_data_file, "r") as f:
            lines = f.readlines()
    except Exception as e:
        import sys
        sys.stderr.write(f"[sdn_topology] не удалось прочитать {metric_data_file}: {e}\n")
        for n in range(1, size + 1):
            matrix["delay"].setdefault(n, {})
            matrix["bandwidth"].setdefault(n, {})
            matrix["loss"].setdefault(n, {})
        return matrix

    pattern = re.compile(r"(\d+)-(\d+):([\d\.]+),([\d\.]+),([\d\.]+)")
    for line in lines:
        m = pattern.search(line)
        if not m:
            continue
        n1, n2 = int(m.group(1)), int(m.group(2))
        d, b, l = float(m.group(3)), float(m.group(4)), float(m.group(5))
        if d > 0:
            matrix["delay"].setdefault(n1, {})[n2] = d
        if b > 0:
            matrix["bandwidth"].setdefault(n1, {})[n2] = b
        if l > 0:
            matrix["loss"].setdefault(n1, {})[n2] = l

    for n in range(1, size + 1):
        matrix["delay"].setdefault(n, {})
        matrix["bandwidth"].setdefault(n, {})
        matrix["loss"].setdefault(n, {})

    return matrix
