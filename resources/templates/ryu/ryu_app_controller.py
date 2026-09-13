"""
SNet — Ryu контроллер. Простой L2 learning switch на основе ryu.app.simple_switch_13
из стандартной библиотеки Ryu. Никакой магии, никакой топологии в packet_in,
никакого OFPFC_DELETE — пакеты пингуются.

Запуск (SNet делает это автоматически):
    ryu-manager --observe-links --wsapi-port 8080 ryu_app_controller.py

WSGI с REST endpoints оставлен — нужен для веб-интерфейса и для отображения
алгоритмов из Qt. Сама маршрутизация — обычный learning switch.
"""

import collections

from ryu.base import app_manager
from ryu.controller import ofp_event
from ryu.controller.handler import (
    CONFIG_DISPATCHER, MAIN_DISPATCHER, set_ev_cls
)
from ryu.ofproto import ofproto_v1_3
from ryu.lib.packet import packet, ethernet, ether_types
from ryu.topology import event as topo_event
from ryu.topology.api import get_link
from ryu.app.wsgi import WSGIApplication
from ryu.lib import hub

import ryu_app_rest_routing

class Controller(app_manager.RyuApp):
    OFP_VERSIONS = [ofproto_v1_3.OFP_VERSION]
    _CONTEXTS = {"wsgi": WSGIApplication}

    def __init__(self, *args, **kwargs):
        super(Controller, self).__init__(*args, **kwargs)
        wsgi = kwargs["wsgi"]
        wsgi.register(ryu_app_rest_routing.ControllerRest, {"controller": self})

        self.logger.info("*** SNet Ryu controller started ***")
        self.logger.info("    Web UI:  http://localhost:8080")

        self.mac_to_port = {}

        self.switches = {}
        self.switches_ports = {}
        self.switches_reachability = {}
        self.host_ports = {}
        self.broadcast_out_ports = {}
        self.host_locations = {}
        self.active_algorithm = None
        self.active_metric = "delay"

        self.flow_stats = {}

        hub.spawn(self._topology_loop)

    def _topology_loop(self):
        while True:
            hub.sleep(2.0)
            try:
                self._recompute_topology()
            except Exception as e:
                self.logger.warning("topology recompute error: %s", e)

    def _recompute_topology(self):
        """Обновляет switches_reachability через ryu.topology.api.get_link().
        Если get_link вернул пусто (transient под нагрузкой) — НЕ затираем кэш."""
        new_reach = {}
        try:
            links = get_link(self, None)
        except Exception:
            links = []
        for link in links:
            new_reach.setdefault(link.src.dpid, {})[link.dst.dpid] = link.src.port_no

        if not new_reach and self.switches_reachability:
            return

        if new_reach != self.switches_reachability:
            self.logger.info("Топология: %d свитчей видят соседей",
                             len(new_reach))
        self.switches_reachability = new_reach

        for dpid, ports in self.switches_ports.items():
            sw_ports = set(new_reach.get(dpid, {}).values())
            self.host_ports[dpid] = set(ports) - sw_ports

        self.broadcast_out_ports = self._build_mst_broadcast(new_reach)

    def _build_mst_broadcast(self, reach):
        """BFS-MST от min(dpid). Возвращает {dpid: set(port)} =
        host-порты + MST switch-to-switch порты. Использование этих
        портов для broadcast устраняет петли в кольце без STP."""
        result = {sw: set(self.host_ports.get(sw, set()))
                  for sw in self.switches.keys()}
        if not reach:
            return result
        root = min(reach.keys())
        visited = {root}
        queue = collections.deque([root])
        while queue:
            sw = queue.popleft()
            for neighbour, out_port in reach.get(sw, {}).items():
                if neighbour in visited:
                    continue
                visited.add(neighbour)
                result.setdefault(sw, set()).add(out_port)
                back_port = reach.get(neighbour, {}).get(sw)
                if back_port is not None:
                    result.setdefault(neighbour, set()).add(back_port)
                queue.append(neighbour)
        return result

    def _broadcast_ports(self, dpid, in_port):
        """Список портов для широковещания, кроме входного."""
        br = self.broadcast_out_ports.get(dpid, set())
        if not br:
            br = set(self.switches_ports.get(dpid, []))
        return [p for p in br if p != in_port]

    @set_ev_cls(topo_event.EventSwitchEnter)
    def _switch_enter(self, ev):
        hub.spawn_after(0.5, self._recompute_topology)

    @set_ev_cls(topo_event.EventLinkAdd)
    def _link_add(self, ev):
        hub.spawn_after(0.5, self._recompute_and_reroute)

    @set_ev_cls(topo_event.EventLinkDelete)
    def _link_delete(self, ev):
        hub.spawn_after(0.5, self._recompute_and_reroute)

    def _recompute_and_reroute(self):
        """Пересчитать топологию и переустановить flow активного алгоритма."""
        self._recompute_topology()
        if self.active_algorithm:
            try:
                self.logger.info("Топология изменилась → пересчёт активной "
                                 "маршрутизации (%s)", self.active_algorithm)
                self._clear_algorithm_flows()
                self._install_algorithm_flows(self.active_algorithm,
                                              self.active_metric)
            except Exception as e:
                self.logger.warning("re-route после изменения топологии "
                                    "провален: %s", e)

    def add_flow(self, datapath, priority, match, actions, buffer_id=None):
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser
        inst = [parser.OFPInstructionActions(ofproto.OFPIT_APPLY_ACTIONS, actions)]
        if buffer_id:
            mod = parser.OFPFlowMod(datapath=datapath, buffer_id=buffer_id,
                                    priority=priority, match=match,
                                    instructions=inst)
        else:
            mod = parser.OFPFlowMod(datapath=datapath, priority=priority,
                                    match=match, instructions=inst)
        datapath.send_msg(mod)

    @set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
    def switch_features_handler(self, ev):
        datapath = ev.msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser
        dpid = datapath.id

        self.logger.info("Switch connected dpid=%s", dpid)
        self.switches[dpid] = datapath
        self.mac_to_port.setdefault(dpid, {})

        match = parser.OFPMatch()
        actions = [parser.OFPActionOutput(ofproto.OFPP_CONTROLLER,
                                          ofproto.OFPCML_NO_BUFFER)]
        self.add_flow(datapath, 0, match, actions)

        req = parser.OFPPortDescStatsRequest(datapath, 0)
        datapath.send_msg(req)

    @set_ev_cls(ofp_event.EventOFPPortDescStatsReply)
    def port_desc_stats_reply_handler(self, ev):
        datapath = ev.msg.datapath
        ofproto = datapath.ofproto
        dpid = datapath.id
        self.switches_ports[dpid] = [
            p.port_no for p in ev.msg.body if p.port_no != ofproto.OFPP_LOCAL
        ]
        hub.spawn_after(0.3, self._recompute_topology)

    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def packet_in_handler(self, ev):
        msg = ev.msg
        datapath = msg.datapath
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser
        in_port = msg.match['in_port']

        pkt = packet.Packet(msg.data)
        eth_layers = pkt.get_protocols(ethernet.ethernet)
        if not eth_layers:
            return
        eth = eth_layers[0]

        if eth.ethertype == ether_types.ETH_TYPE_LLDP:
            return

        dst = eth.dst
        src = eth.src
        dpid = datapath.id
        self.mac_to_port.setdefault(dpid, {})

        host_ports = self.host_ports.get(dpid)
        if host_ports is None or in_port in host_ports:
            self.mac_to_port[dpid][src] = in_port
            self.host_locations[src] = (dpid, in_port)

        if dst in self.mac_to_port[dpid]:
            out_port = self.mac_to_port[dpid][dst]
            actions = [parser.OFPActionOutput(out_port)]
            match = parser.OFPMatch(in_port=in_port, eth_dst=dst, eth_src=src)
            if msg.buffer_id != ofproto.OFP_NO_BUFFER:
                self.add_flow(datapath, 1, match, actions, msg.buffer_id)
                return
            else:
                self.add_flow(datapath, 1, match, actions)
        else:
            out_ports = self._broadcast_ports(dpid, in_port)
            actions = [parser.OFPActionOutput(p) for p in out_ports]

        if not actions:
            return

        data = None
        if msg.buffer_id == ofproto.OFP_NO_BUFFER:
            data = msg.data

        out = parser.OFPPacketOut(datapath=datapath, buffer_id=msg.buffer_id,
                                  in_port=in_port, actions=actions, data=data)
        datapath.send_msg(out)

    ALGO_COOKIE = 0xA160C0DE
    ALGO_PRIORITY = 20

    def set_active_algorithm(self, name, metric="delay"):
        self._clear_algorithm_flows()
        self.active_algorithm = name
        self.active_metric = metric
        if not name:
            self.logger.info("Активная маршрутизация выключена → L2 learning")
            return
        self.logger.info("Активная маршрутизация: алгоритм=%s метрика=%s",
                         name, metric)
        try:
            self._install_algorithm_flows(name, metric)
        except Exception as e:
            self.logger.warning("install_algorithm_flows провален: %s", e)

    def _clear_algorithm_flows(self):
        """Удаляет все flow-правила с нашим cookie на всех известных
        свитчах. Использует OFPFC_DELETE с cookie_mask."""
        for dpid, datapath in self.switches.items():
            ofproto = datapath.ofproto
            parser = datapath.ofproto_parser
            mod = parser.OFPFlowMod(
                datapath=datapath,
                cookie=self.ALGO_COOKIE,
                cookie_mask=0xFFFFFFFFFFFFFFFF,
                table_id=ofproto.OFPTT_ALL,
                command=ofproto.OFPFC_DELETE,
                out_port=ofproto.OFPP_ANY,
                out_group=ofproto.OFPG_ANY,
                match=parser.OFPMatch(),
            )
            datapath.send_msg(mod)

    def _install_algo_flow(self, datapath, eth_src, eth_dst, out_port):
        parser = datapath.ofproto_parser
        ofproto = datapath.ofproto
        match = parser.OFPMatch(eth_src=eth_src, eth_dst=eth_dst)
        actions = [parser.OFPActionOutput(out_port)]
        inst = [parser.OFPInstructionActions(ofproto.OFPIT_APPLY_ACTIONS, actions)]
        mod = parser.OFPFlowMod(
            datapath=datapath,
            cookie=self.ALGO_COOKIE,
            priority=self.ALGO_PRIORITY,
            match=match,
            instructions=inst,
        )
        datapath.send_msg(mod)

    def _path_via_algorithm(self, name, metric, src_dpid, dst_dpid):
        """Считает путь src_dpid → dst_dpid через выбранный алгоритм.
        Возвращает список dpid'ов или None если путь не найден."""
        try:
            import algorithms as builtin
        except Exception:
            return None
        algo_cls = None
        for cls in builtin.BUILTIN_ALGORITHMS:
            if cls.name == name:
                algo_cls = cls
                break
        if algo_cls is None:
            self.logger.warning("Алгоритм '%s' не найден среди встроенных", name)
            return None

        graph = {}
        for s1, neighbours in self.switches_reachability.items():
            for s2 in neighbours:
                w = 1
                graph.setdefault(s1, {})[s2] = w
                graph.setdefault(s2, {})[s1] = w
        graphs = {"delay": graph, "bandwidth": graph, "loss": graph}

        algo = algo_cls()
        params = {}
        for pname, meta in algo_cls.params.items():
            default = meta[1] if isinstance(meta, tuple) and len(meta) > 1 else None
            params[pname] = default
        params["metric"] = metric
        try:
            result = algo.run(src_dpid, dst_dpid, graphs, params)
        except Exception as e:
            self.logger.warning("Алгоритм %s упал на (%s→%s): %s",
                                name, src_dpid, dst_dpid, e)
            return None

        routes = result.get("routes") if isinstance(result, dict) else None
        if routes:
            for path in routes:
                if path and path[0] == src_dpid and path[-1] == dst_dpid:
                    return path
            return routes[0] if routes else None
        tree = result.get("tree") if isinstance(result, dict) else None
        if tree:
            parent = {}
            for a, b in tree:
                parent[b] = a
            cur = dst_dpid
            path = [cur]
            visited = {cur}
            while cur != src_dpid:
                if cur not in parent or parent[cur] in visited:
                    return None
                cur = parent[cur]
                path.append(cur)
                visited.add(cur)
            path.reverse()
            return path
        return None

    def _install_algorithm_flows(self, name, metric):
        """Для каждой пары известных хостов: считаем путь и ставим flow
        на каждый свитч вдоль пути (в обе стороны)."""
        if not self.switches_reachability:
            self.logger.warning("Нет данных о топологии — пропуск установки flow")
            return
        hosts = list(self.host_locations.items())
        if len(hosts) < 2:
            self.logger.info("Известно меньше 2 хостов — нечего маршрутизировать")
            return
        total_flows = 0
        for i in range(len(hosts)):
            for j in range(len(hosts)):
                if i == j:
                    continue
                mac_src, (dpid_src, port_src) = hosts[i]
                mac_dst, (dpid_dst, port_dst) = hosts[j]
                if dpid_src == dpid_dst:
                    datapath = self.switches.get(dpid_src)
                    if datapath:
                        self._install_algo_flow(datapath, mac_src, mac_dst, port_dst)
                        total_flows += 1
                    continue
                path = self._path_via_algorithm(name, metric, dpid_src, dpid_dst)
                if not path:
                    self.logger.warning("Нет пути %s → %s через %s",
                                        dpid_src, dpid_dst, name)
                    continue
                for k, dpid in enumerate(path):
                    datapath = self.switches.get(dpid)
                    if not datapath:
                        continue
                    if k == len(path) - 1:
                        out_port = port_dst
                    else:
                        next_dpid = path[k + 1]
                        out_port = self.switches_reachability.get(dpid, {}).get(next_dpid)
                        if out_port is None:
                            self.logger.warning(
                                "Нет порта s%s→s%s, разрываем установку пути",
                                dpid, next_dpid)
                            break
                    self._install_algo_flow(datapath, mac_src, mac_dst, out_port)
                    total_flows += 1
        self.logger.info("Активная маршрутизация: установлено %d flow-правил",
                         total_flows)

    def request_flow_stats(self):
        """Запрашивает OFPFlowStatsRequest у всех известных свитчей.
        Ответы приходят асинхронно в _flow_stats_reply и складываются в
        self.flow_stats. Веб-обработчик вызывает этот метод, ждёт ~0.5с
        через hub.sleep и читает кэш."""
        for dpid, datapath in self.switches.items():
            parser = datapath.ofproto_parser
            req = parser.OFPFlowStatsRequest(datapath)
            datapath.send_msg(req)

    @set_ev_cls(ofp_event.EventOFPFlowStatsReply, MAIN_DISPATCHER)
    def _flow_stats_reply(self, ev):
        dpid = ev.msg.datapath.id
        rows = []
        for stat in ev.msg.body:
            try:
                match = dict(stat.match.items())
            except Exception:
                match = {}
            actions = []
            try:
                for inst in stat.instructions:
                    for act in getattr(inst, "actions", []):
                        port = getattr(act, "port", None)
                        if port is not None:
                            actions.append("output:%s" % port)
                        else:
                            actions.append(act.__class__.__name__)
            except Exception:
                pass
            rows.append({
                "cookie": "0x%x" % stat.cookie,
                "priority": stat.priority,
                "table_id": stat.table_id,
                "match": match,
                "actions": actions,
                "packet_count": stat.packet_count,
                "byte_count": stat.byte_count,
            })
        rows.sort(key=lambda r: r["priority"], reverse=True)
        self.flow_stats[dpid] = rows
