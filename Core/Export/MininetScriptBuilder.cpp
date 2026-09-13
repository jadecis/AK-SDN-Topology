#include "MininetScriptBuilder.h"
#include "Switch.h"
#include "Host.h"
#include "DockerNode.h"
#include "SdnController.h"
#include "SSLink.h"
#include "Node.h"

MininetScriptBuilder::MininetScriptBuilder(PortMatrix portMatrix) :
    portMatrix(portMatrix)
{
}

void MininetScriptBuilder::addSwitchData(Switch *sw)
{

    switchesData.append(QString("%0 = net.addSwitch('%0', protocols='OpenFlow13')")
                                .arg(sw->getName()));
    startingData.append(QString("%0.start([c0])").arg(sw->getName()));
}

void MininetScriptBuilder::addSdnControllerData(SdnController *controller)
{
    sdnControllersData.append
            (QString("%0 = net.addController('%0', controller=RemoteController, ip='%1', port=%2)")
                .arg(controller->getName()).arg(controller->getIp()).arg(controller->getPort()));
    startingData.insert(0, QString("%0.start()").arg(controller->getName()));
}

void MininetScriptBuilder::addHostData(Host *host)
{
    hostsData.append(QString("%0 = net.addHost('%0', mac='%1', ip='%2')").arg(
                             host->getName(),
                             host->getMac(),
                             host->getIp()));
}

void MininetScriptBuilder::addDockerNodeData(DockerNode *d)
{

    QStringList kwargs;
    kwargs << QString("ip='%1'").arg(d->getIp());
    kwargs << QString("dimage='%1'").arg(d->getImage());
    if (!d->getCommand().isEmpty())
        kwargs << QString("dcmd=\"%1\"").arg(d->getCommand());

    QStringList envPairs;
    for (const QString &kv : d->getEnvironment())
    {
        int eq = kv.indexOf('=');
        if (eq <= 0) continue;
        envPairs << QString("'%1':'%2'").arg(kv.left(eq), kv.mid(eq + 1));
    }
    if (!envPairs.isEmpty())
        kwargs << QString("environment={%1}").arg(envPairs.join(','));

    if (!d->getVolumes().isEmpty())
    {
        QStringList vols;
        for (const QString &v : d->getVolumes())
            vols << QString("'%1'").arg(v);
        kwargs << QString("volumes=[%1]").arg(vols.join(','));
    }

    if (!d->getPublishedPorts().isEmpty())
    {
        QStringList pbPairs, exposedPorts;
        for (const QString &p : d->getPublishedPorts())
        {
            int colon = p.indexOf(':');
            if (colon <= 0 || colon == p.length() - 1) continue;
            QString hostPort      = p.left(colon).trimmed();
            QString containerPort = p.mid(colon + 1).trimmed();
            bool okH = false, okC = false;
            hostPort.toInt(&okH);
            containerPort.toInt(&okC);
            if (!okH || !okC) continue;
            pbPairs      << QString("%1: %2").arg(containerPort, hostPort);
            exposedPorts << containerPort;
        }
        if (!pbPairs.isEmpty())
        {
            kwargs << QString("port_bindings={%1}").arg(pbPairs.join(", "));
            kwargs << QString("ports=[%1]").arg(exposedPorts.join(", "));
        }
    }

    dockerNodesData.append(QString("%0 = net.addDocker('%0', %1)")
                           .arg(d->getName(), kwargs.join(", ")));

    dockerSetupData.append(qMakePair(d->getName(), d->getIp()));

    if (!d->getImage().isEmpty()) dockerImagesSet_.insert(d->getImage());
}

void MininetScriptBuilder::addSSLinkData(SSLink *link)
{
    Node *node1 = link->getNode1();
    Node *node2 = link->getNode2();

    linksData.append(QString("net.addLink(%0, %1, %2, %3, bw=%4, delay='%5ms', loss=%6)").arg(
                             node1->getName(),
                             node2->getName(),
                             QString::number(portMatrix.getPortNumber(node1, node2)),
                             QString::number(portMatrix.getPortNumber(node2, node1)),
                             QString::number(link->getBandwidth()),
                             QString::number(link->getDelay()),
                             QString::number(link->getPacketLoss())));
}

QString MininetScriptBuilder::buildIncludeBlock()
{
    QStringList includeBlock;
    includeBlock.append("#!/usr/bin/env python3");
    includeBlock.append("# -*- coding: utf-8 -*-");
    includeBlock.append("# Сгенерировано SDN Topology (SNet)");
    includeBlock.append("");
    includeBlock.append("import argparse");
    includeBlock.append("import json");
    includeBlock.append("import re");
    includeBlock.append("import sys");
    includeBlock.append("import threading");
    includeBlock.append("import time");
    includeBlock.append("import collections");
    includeBlock.append("from http.server import BaseHTTPRequestHandler, HTTPServer");

    includeBlock.append("from socketserver import ThreadingMixIn");
    includeBlock.append("class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):");
    includeBlock.append("    daemon_threads = True");
    includeBlock.append("    allow_reuse_address = True");
    includeBlock.append("");
    if (useContainernet_)
    {

        includeBlock.append("from mininet.node import Controller, RemoteController, OVSKernelSwitch, Docker");
        includeBlock.append("from mininet.cli import CLI");
        includeBlock.append("from mininet.log import setLogLevel");
        includeBlock.append("from mininet.link import TCLink, Link");
        includeBlock.append("try:");
        includeBlock.append("\tfrom mininet.net import Containernet as Mininet");
        includeBlock.append("except ImportError:");
        includeBlock.append("\ttry:");
        includeBlock.append("\t\tfrom containernet.net import Containernet as Mininet");
        includeBlock.append("\texcept ImportError:");
        includeBlock.append("\t\timport sys");
        includeBlock.append("\t\tsys.stderr.write('!! Containernet не установлен. См. README.\\n')");
        includeBlock.append("\t\tsys.exit(1)");
    }
    else
    {
        includeBlock.append("from mininet.net import Mininet");
        includeBlock.append("from mininet.node import Controller, RemoteController, OVSKernelSwitch");
        includeBlock.append("from mininet.cli import CLI");
        includeBlock.append("from mininet.log import setLogLevel");
        includeBlock.append("from mininet.link import TCLink");
    }
    return includeBlock.join("\n").append("\n\n");
}

QString MininetScriptBuilder::buildFunctionBeginBlock()
{
    QStringList functionBeginBlock;
    functionBeginBlock.append("def topology(http_port=0, run_pingall=False):");
    functionBeginBlock.append("net = Mininet(controller=RemoteController, link=TCLink, switch=OVSKernelSwitch)");
    return functionBeginBlock.join("\n\t").append("\n\n");
}

QString MininetScriptBuilder::buildSdnControllersBlock()
{
    return QString("\t").append(sdnControllersData.join("\n\t").append("\n\n"));
}

QString MininetScriptBuilder::buildHostsBlock()
{
    return QString("\t").append(hostsData.join("\n\t").append("\n\n"));
}

QString MininetScriptBuilder::buildDockerNodesBlock()
{
    if (dockerNodesData.isEmpty()) return QString();
    QStringList block;

    if (!dockerImagesSet_.isEmpty())
    {
        block.append("# === Пред-pull Docker-образов (CLI вместо dcli.pull) ===");
        block.append("import subprocess as _subp_pull");
        QStringList imgList;
        for (const QString &img : dockerImagesSet_) imgList << QString("'%1'").arg(img);
        block.append(QString("_images = [%1]").arg(imgList.join(", ")));
        block.append("for _img in _images:");
        block.append("\tprint('[SNet] pulling %s ...' % _img, flush=True)");
        block.append("\t_r = _subp_pull.run(['docker', 'pull', _img],");
        block.append("\t                    capture_output=True, text=True, timeout=600)");
        block.append("\tif _r.returncode != 0:");
        block.append("\t\tprint('[SNet] !! pull %s FAILED rc=%d:' % (_img, _r.returncode), flush=True)");
        block.append("\t\tprint(_r.stderr, flush=True)");
        block.append("\telse:");

        block.append("\t\t_status = [l for l in _r.stdout.splitlines() if l.strip()]");
        block.append("\t\tprint('[SNet] %s -> %s' % (_img, _status[-1] if _status else 'OK'), flush=True)");
        block.append("");
    }

    block.append(dockerNodesData);

    block.append("");
    block.append("# === Pre-install tc/iproute2 в Docker-контейнерах (нужно для TCLink) ===");
    block.append("# Поддерживаем apt-get (Debian/Ubuntu), apk (Alpine), microdnf/dnf/yum");
    block.append("# (RHEL/Oracle Linux — на нём базируется mysql:8 и др.).");
    block.append("import subprocess as _subp_pre");
    for (const auto &pair : dockerSetupData)
    {
        const QString &name = pair.first;
        block.append(QString("print('[SNet] %1: pre-install iproute2/ping...', flush=True)").arg(name));
        /* ВАЖНО: шаг best-effort. Раньше при недоступной сети внутри контейнера
         * apt/dnf висели до timeout, а subprocess.TimeoutExpired НЕ ловился и
         * ронял всю топологию (типичный случай — mysql:8 на Oracle Linux, d3).
         * Теперь любое исключение перехватывается: контейнер просто остаётся
         * без tc (link с минимальным шейпом всё равно поднимется). Таймаут
         * снижен до 90 c, чтобы не ждать по 3 минуты на каждый сбой. */
        block.append(QString("try:"));
        block.append(QString("\t_r = _subp_pre.run(["
                             "'docker', 'exec', 'mn.%1', 'sh', '-c',"
                             "'which tc >/dev/null 2>&1 && echo TC_OK || ("
                             "(command -v apt-get >/dev/null 2>&1 && apt-get update >/dev/null 2>&1 && apt-get install -y iproute2 iputils-ping >/dev/null 2>&1) || "
                             "(command -v apk      >/dev/null 2>&1 && apk add --no-cache iproute2 iputils >/dev/null 2>&1) || "
                             "(command -v microdnf >/dev/null 2>&1 && microdnf install -y iproute iputils >/dev/null 2>&1) || "
                             "(command -v dnf      >/dev/null 2>&1 && dnf install -y iproute iputils >/dev/null 2>&1) || "
                             "(command -v yum      >/dev/null 2>&1 && yum install -y iproute iputils >/dev/null 2>&1) || "
                             "echo NO_PKG_MGR; "
                             "which tc >/dev/null 2>&1 && echo TC_OK || echo TC_FAIL"
                             ")'"
                             "], capture_output=True, text=True, timeout=90)").arg(name));
        block.append(QString("\t_out_%1 = (_r.stdout or '') + (_r.stderr or '')").arg(name));
        block.append(QString("\t_rc_%1 = _r.returncode").arg(name));
        block.append(QString("except Exception as _e:"));
        block.append(QString("\t_out_%1 = 'TC_TIMEOUT'").arg(name));
        block.append(QString("\t_rc_%1 = -1").arg(name));
        block.append(QString("\tprint('[SNet] !! %1: pre-install прерван (' + type(_e).__name__ + ') — пропускаю, топология продолжит запуск', flush=True)").arg(name));
        block.append(QString("if 'TC_OK' in _out_%1:").arg(name));
        block.append(QString("\tprint('[SNet] %1: iproute2 OK', flush=True)").arg(name));
        block.append(QString("elif 'NO_PKG_MGR' in _out_%1:").arg(name));
        block.append(QString("\tprint('[SNet] !! %1: нет apt/apk/dnf/yum — поставьте tc в образ заранее', flush=True)").arg(name));
        block.append(QString("else:"));
        block.append(QString("\tprint('[SNet] !! %1: tc не установлен (rc=' + str(_rc_%1) + '), link может не шейпиться', flush=True)").arg(name));
    }
    block.append("");
    return QString("\t").append(block.join("\n\t")).append("\n");
}

QString MininetScriptBuilder::buildSwitchesBlock()
{
    return QString("\t").append(switchesData.join("\n\t").append("\n\n"));
}

QString MininetScriptBuilder::buildLinksBlock()
{
    return QString("\t").append(linksData.join("\n\t").append("\n\n"));
}

QString MininetScriptBuilder::buildStartingBlock()
{
    return QString("\t").append(startingData.join("\n\t").append("\n\n"));
}

QString MininetScriptBuilder::buildFunctionEndBlock()
{

    QStringList block;
    block.append("");

    if (!dockerSetupData.isEmpty())
    {
        block.append("# === Активация Containernet-интерфейсов Docker-узлов ===");
        block.append("# Containernet не настраивает эти интерфейсы автоматически.");
        block.append("# Используем docker exec через subprocess напрямую — Containernet'овский");
        block.append("# d1.cmd() иногда тихо не доходит до контейнера (внутренний shell).");
        block.append("import subprocess as _subp");
        block.append("def _setup_docker_iface(name, ip):");
        block.append("    container = 'mn.' + name");
        block.append("    # Containernet называет интерфейсы по-разному в разных версиях:");
        block.append("    # \"d1-eth0\", \"d1-eth0-1\" (с port-суффиксом), и т.п.");
        block.append("    # Ищем первый интерфейс начинающийся на <name>- и не являющийся lo/eth0.");
        block.append("    iface = None");
        block.append("    intf_ready = False");
        block.append("    for _i in range(40):");
        block.append("        r = _subp.run(['docker', 'exec', container, 'ip', '-o', 'link', 'show'],");
        block.append("                      capture_output=True, text=True, timeout=5)");
        block.append("        if r.returncode == 0:");
        block.append("            for _line in (r.stdout or '').split('\\n'):");
        block.append("                _m = re.match(r'^\\d+:\\s+([^@:\\s]+)', _line)");
        block.append("                if not _m: continue");
        block.append("                _ifn = _m.group(1)");
        block.append("                if _ifn in ('lo', 'eth0'): continue");
        block.append("                if _ifn.startswith(name + '-') or _ifn.startswith(name):");
        block.append("                    iface = _ifn; intf_ready = True; break");
        block.append("        if intf_ready: break");
        block.append("        time.sleep(0.25)");
        block.append("    if intf_ready:");
        block.append("        print('[SNet] ' + container + ': найден SDN-интерфейс ' + iface)");
        block.append("    if not intf_ready:");
        block.append("        # iface может быть None — печатаем сообщение БЕЗ конкатенации с None");
        block.append("        print('[SNet] !! SDN-интерфейс не появился в ' + container)");
        block.append("        # === ДИАГНОСТИКА ===");
        block.append("        st = _subp.run(['docker', 'inspect', '--format',");
        block.append("                        'pid={{.State.Pid}} status={{.State.Status}} restart={{.RestartCount}}',");
        block.append("                        container], capture_output=True, text=True, timeout=5)");
        block.append("        print('[SNet] DIAG ' + container + ': ' + st.stdout.strip())");
        block.append("        ic = _subp.run(['docker', 'exec', container, 'ip', '-o', 'link', 'show'],");
        block.append("                       capture_output=True, text=True, timeout=5)");
        block.append("        print('[SNet] DIAG interfaces inside ' + container + ':')");
        block.append("        for _l in (ic.stdout or '').split('\\n')[:10]:");
        block.append("            if _l.strip(): print('  ' + _l.strip()[:150])");
        block.append("        ih = _subp.run(['ip', '-o', 'link', 'show'], capture_output=True, text=True, timeout=5)");
        block.append("        d_lines = [_l for _l in (ih.stdout or '').split('\\n') if (name + '-') in _l or ('-' + name) in _l]");
        block.append("        if d_lines:");
        block.append("            print('[SNet] DIAG ' + name + '-связанные интерфейсы на ХОСТЕ (veth не переехал в namespace!):')");
        block.append("            for _l in d_lines[:5]: print('  ' + _l.strip()[:150])");
        block.append("            # Извлекаем реальное имя host-side интерфейса из первой строки");
        block.append("            # ('123: d1-eth0@if124: ...' → 'd1-eth0').");
        block.append("            _mh = re.match(r'^\\d+:\\s+([^@:\\s]+)', d_lines[0])");
        block.append("            _host_iface = _mh.group(1) if _mh else None");
        block.append("            # Пробуем вручную перенести в namespace контейнера");
        block.append("            pid_r = _subp.run(['docker', 'inspect', '--format', '{{.State.Pid}}', container],");
        block.append("                              capture_output=True, text=True, timeout=5)");
        block.append("            cpid = (pid_r.stdout or '').strip()");
        block.append("            if _host_iface and cpid and cpid != '0':");
        block.append("                print('[SNet] DIAG try manual move ' + _host_iface + ' -> netns pid=' + cpid)");
        block.append("                mv = _subp.run(['ip', 'link', 'set', _host_iface, 'netns', cpid],");
        block.append("                               capture_output=True, text=True, timeout=5)");
        block.append("                if mv.returncode == 0:");
        block.append("                    print('[SNet] DIAG manual netns move OK')");
        block.append("                    iface = _host_iface");
        block.append("                    intf_ready = True");
        block.append("                else:");
        block.append("                    print('[SNet] DIAG manual move FAILED: ' + (mv.stderr or '').strip())");
        block.append("        if not intf_ready:");
        block.append("            print('[SNet] !! ' + container + ': не удалось настроить интерфейс, пропускаем')");
        block.append("            return False");
        block.append("    # Очистка + установка IP + подъём интерфейса");
        block.append("    _subp.run(['docker', 'exec', container, 'ip', '-4', 'addr', 'flush', 'dev', iface],");
        block.append("              capture_output=True, timeout=5)");
        block.append("    r1 = _subp.run(['docker', 'exec', container, 'ip', 'addr', 'add', ip + '/8', 'dev', iface],");
        block.append("                   capture_output=True, text=True, timeout=5)");
        block.append("    r2 = _subp.run(['docker', 'exec', container, 'ip', 'link', 'set', iface, 'up'],");
        block.append("                   capture_output=True, text=True, timeout=5)");
        block.append("    if r1.returncode != 0:");
        block.append("        print('[SNet] !! ' + name + ' ip addr add error: ' + (r1.stderr or '').strip())");
        block.append("    if r2.returncode != 0:");
        block.append("        print('[SNet] !! ' + name + ' ip link up error: ' + (r2.stderr or '').strip())");
        block.append("    # Удалить default route через docker0 (172.17.0.1), чтобы трафик в 10.0.0.0/8");
        block.append("    # шёл через d1-eth0, иначе ARP/ping не пойдёт через SDN-сеть");
        block.append("    _subp.run(['docker', 'exec', container, 'sh', '-c',");
        block.append("               'ip route del default 2>/dev/null; ip route add 10.0.0.0/8 dev ' + iface],");
        block.append("              capture_output=True, timeout=5)");
        block.append("    # Доустановить iputils-ping/iproute2, если есть apt (ubuntu/debian)");
        block.append("    _subp.run(['docker', 'exec', container, 'sh', '-c',");
        block.append("               'which ping >/dev/null 2>&1 || (apt-get update >/dev/null 2>&1 && apt-get install -y iputils-ping iproute2 >/dev/null 2>&1) || (apk add iputils iproute2 >/dev/null 2>&1) || true'],");
        block.append("              capture_output=True, timeout=60)");
        block.append("    # Финальный статус");
        block.append("    chk = _subp.run(['docker', 'exec', container, 'ip', '-4', 'addr', 'show', iface],");
        block.append("                    capture_output=True, text=True, timeout=5)");
        block.append("    has_ip = (ip in (chk.stdout or ''))");
        block.append("    print('[SNet] ' + iface + ' setup ' + ('OK ' + ip + '/8' if has_ip else 'FAILED: ' + (chk.stdout or '').strip()[:200]))");
        block.append("    return has_ip");
        block.append("");
        for (const auto &pair : dockerSetupData)
        {
            const QString &name = pair.first;
            const QString &ip = pair.second;
            block.append(QString("_setup_docker_iface('%1', '%2')").arg(name, ip));
        }
        block.append("time.sleep(1)");
        block.append("");

        block.append("# === Проверка и принудительная переустановка controller на всех свитчах ===");
        block.append("for sw in net.switches:");
        block.append("\ttry:");
        block.append("\t\texists = sw.cmd('ovs-vsctl br-exists ' + sw.name + ' && echo YES || echo NO').strip()");
        block.append("\t\tif 'NO' in exists:");
        block.append("\t\t\tprint('[SNet] !! ' + sw.name + ' OVS bridge MISSING — recreating')");
        block.append("\t\t\tsw.cmd('ovs-vsctl --may-exist add-br ' + sw.name)");
        block.append("\t\t\t# Восстанавливаем корректный dpid, чтобы Ryu видел нормальный номер свитча");
        block.append("\t\t\tdpid_hex = sw.dpid if (hasattr(sw, 'dpid') and sw.dpid) else format(int(sw.name[1:]) if sw.name[1:].isdigit() else 1, '016x')");
        block.append("\t\t\tsw.cmd('ovs-vsctl set bridge ' + sw.name + ' other-config:datapath-id=' + dpid_hex)");
        block.append("\t\t\tsw.cmd('ovs-vsctl set bridge ' + sw.name + ' protocols=OpenFlow13')");
        block.append("\t\t\t# Перепривязываем интерфейсы к мосту");
        block.append("\t\t\tfor intf in sw.intfList():");
        block.append("\t\t\t\tif intf.name and intf.name != 'lo':");
        block.append("\t\t\t\t\tsw.cmd('ovs-vsctl --may-exist add-port ' + sw.name + ' ' + intf.name)");
        block.append("\t\tsw.cmd('ovs-vsctl set-controller ' + sw.name + ' tcp:127.0.0.1:6653')");
        block.append("\t\tsw.cmd('ovs-vsctl set bridge ' + sw.name + ' protocols=OpenFlow13')");
        block.append("\t\tprint('[SNet] %s: controller=tcp:127.0.0.1:6653 (OF1.3)' % sw.name)");
        block.append("\texcept Exception as _e:");
        block.append("\t\tprint('[SNet] set-controller failed for %s: %s' % (sw.name, _e))");
        block.append("time.sleep(2)  # дать свитчам подключиться");
        block.append("");
    }
    block.append("");
    block.append("if http_port:");
    block.append("\tserver, bound_port = start_http_server(net, http_port)");
    block.append("\tif server: print('[SNet] HTTP server listening on 127.0.0.1:%d' % bound_port)");
    block.append("else:");
    block.append("\tserver = None");
    block.append("");
    block.append("# tcpdump-монитор траффика для анимации в GUI");
    block.append("traffic_thread = start_traffic_monitor(net)");
    block.append("");
    block.append("if run_pingall:");
    block.append("\tresult = do_pingall(net)");
    block.append("\tprint('SNET_PINGALL_JSON:' + json.dumps(result))");
    block.append("\tnet.stop()");
    block.append("\tif server: server.shutdown()");
    block.append("\treturn");
    block.append("");
    block.append("CLI(net)");
    block.append("if server: server.shutdown()");
    block.append("net.stop()");
    return QString("\t").append(block.join("\n\t")).append("\n\n");
}

QString MininetScriptBuilder::buildStartupBlock()
{
    QStringList block;
    block.append("");
    block.append("# ---------- compat shim для Python 3.10+ (collections.Mapping etc.) ----------");
    block.append("for _n in ('Mapping','MutableMapping','Hashable','Iterable','Container','Sized','Callable'):");
    block.append("    if not hasattr(collections, _n):");
    block.append("        import collections.abc");
    block.append("        setattr(collections, _n, getattr(collections.abc, _n))");
    block.append("");
    block.append("# ---------- очередь событий трафика для GUI-анимации ----------");
    block.append("TRAFFIC_EVENTS = []  # [{ts, src, dst, kind}]");
    block.append("TRAFFIC_LOCK = threading.Lock()");
    block.append("TRAFFIC_MAX = 500");
    block.append("");
    block.append("def push_event(src, dst, kind):");
    block.append("    with TRAFFIC_LOCK:");
    block.append("        TRAFFIC_EVENTS.append({'ts': time.time(), 'src': src, 'dst': dst, 'kind': kind})");
    block.append("        if len(TRAFFIC_EVENTS) > TRAFFIC_MAX:");
    block.append("            del TRAFFIC_EVENTS[:len(TRAFFIC_EVENTS) - TRAFFIC_MAX]");
    block.append("");
    block.append("def drain_events(since_ts=0.0):");
    block.append("    with TRAFFIC_LOCK:");
    block.append("        evs = [e for e in TRAFFIC_EVENTS if e['ts'] > since_ts]");
    block.append("        return evs");
    block.append("");
    block.append("def _ip_to_host(net, ip):");
    block.append("    for h in net.hosts:");
    block.append("        if h.IP() == ip:");
    block.append("            return h.name");
    block.append("    return None");
    block.append("");
    block.append("def start_traffic_monitor(net):");
    block.append("    \"\"\"Запускает tcpdump в фоновом потоке и парсит ICMP/iperf события.");
    block.append("    Слушает каждый switch-интерфейс отдельно — это надёжнее чем -i any.\"\"\"");
    block.append("    import subprocess, shutil");
    block.append("    if not shutil.which('tcpdump'):");
    block.append("        print('[SNet] !! tcpdump не найден. Установите: sudo apt install tcpdump')");
    block.append("        return None");
    block.append("    # Собираем список интерфейсов всех свитчей (s1-eth1, s1-eth2, ...)");
    block.append("    intfs = []");
    block.append("    for sw in net.switches:");
    block.append("        for intf in sw.intfList():");
    block.append("            if intf.name and intf.name != 'lo':");
    block.append("                intfs.append(intf.name)");
    block.append("    if not intfs:");
    block.append("        print('[SNet] !! Нет интерфейсов для tcpdump-мониторинга')");
    block.append("        return None");
    block.append("    print('[SNet] tcpdump monitoring %d ifaces (-i any): %s' % (len(intfs), ' '.join(intfs[:8]) + ('...' if len(intfs)>8 else '')))");
    block.append("    def runner():");
    block.append("        try:");
    block.append("            # ВАЖНО: стандартный tcpdump учитывает только ПОСЛЕДНИЙ -i,");
    block.append("            # поэтому несколько '-i s1-eth1 -i s1-eth2 ...' мониторили лишь");
    block.append("            # один интерфейс (анимация шла только для трафика через него).");
    block.append("            # '-i any' слушает ВСЕ интерфейсы → события для любой пары хостов.");
    block.append("            # '-Q in' убран: на 'any' направление ненадёжно; дубликаты");
    block.append("            # (один пакет на нескольких интерфейсах) гасит дебаунс в GUI.");
    block.append("            cmd = ['tcpdump', '-i', 'any', '-l', '-nn',");
    block.append("                   '(icmp or (tcp and (port 5001 or port 80)))']");
    block.append("            p = subprocess.Popen(cmd, stdout=subprocess.PIPE,");
    block.append("                                  stderr=subprocess.PIPE,");
    block.append("                                  bufsize=1, universal_newlines=True)");
    block.append("            print('[SNet] tcpdump monitor started, pid=%d' % p.pid)");
    block.append("            event_count = 0");
    block.append("            for line in p.stdout:");
    block.append("                m = re.search(r'IP6?\\s+([\\d\\.]+)(?:\\.\\d+)?\\s*>\\s*([\\d\\.]+)(?:\\.\\d+)?\\s*:\\s*(.+)', line)");
    block.append("                if not m: continue");
    block.append("                src_ip, dst_ip, rest = m.group(1), m.group(2), m.group(3)");
    block.append("                src = _ip_to_host(net, src_ip)");
    block.append("                dst = _ip_to_host(net, dst_ip)");
    block.append("                if not src or not dst: continue");
    block.append("                if src == dst: continue");
    block.append("                kind = 'icmp' if 'ICMP' in rest else ('iperf' if '5001' in line else 'tcp')");
    block.append("                push_event(src, dst, kind)");
    block.append("                event_count += 1");
    block.append("                if event_count <= 3:");
    block.append("                    print('[SNet] traffic event %s->%s (%s)' % (src, dst, kind))");
    block.append("        except FileNotFoundError:");
    block.append("            print('[SNet] !! tcpdump бинарник недоступен')");
    block.append("        except Exception as e:");
    block.append("            print('[SNet] !! tcpdump monitor exception: ' + str(e))");
    block.append("    t = threading.Thread(target=runner, daemon=True)");
    block.append("    t.start()");
    block.append("    return t");
    block.append("");
    block.append("# ---------- HTTP-сервер для GUI ----------");
    block.append("def _ping_one(net, h1_name, h2_name):");
    block.append("    h1 = net.get(h1_name); h2 = net.get(h2_name)");
    block.append("    ip2 = h2.IP()");
    block.append("    raw = h1.cmd('ping -c 4 -W 1 ' + ip2)");
    block.append("    loss = 100.0; rtt = -1.0");
    block.append("    m = re.search(r'(\\d+)% packet loss', raw)");
    block.append("    if m: loss = float(m.group(1))");
    block.append("    m = re.search(r'rtt[^=]*= [\\d\\.]+/([\\d\\.]+)/', raw)");
    block.append("    if m: rtt = float(m.group(1))");
    block.append("    return {'rtt_ms': rtt, 'loss': loss, 'raw': raw[-2000:]}");
    block.append("");
    block.append("def do_pingall(net):");
    block.append("    hosts = net.hosts");
    block.append("    matrix = []");
    block.append("    for h1 in hosts:");
    block.append("        for h2 in hosts:");
    block.append("            if h1 is h2: continue");
    block.append("            res = _ping_one(net, h1.name, h2.name)");
    block.append("            matrix.append({'src': h1.name, 'dst': h2.name, 'rtt_ms': res['rtt_ms'], 'loss': res['loss']})");
    block.append("    return {'matrix': matrix}");
    block.append("");
    block.append("def _iperf_pair(net, h1_name, h2_name):");
    block.append("    h1 = net.get(h1_name); h2 = net.get(h2_name)");
    block.append("    try:");
    block.append("        result = net.iperf((h1, h2), seconds=2)");
    block.append("        def parse(s):");
    block.append("            m = re.search(r'([\\d\\.]+)\\s*([KMG])?bits', str(s))");
    block.append("            if not m: return 0.0");
    block.append("            v = float(m.group(1)); unit = m.group(2)");
    block.append("            if unit == 'K': v /= 1000.0");
    block.append("            elif unit == 'G': v *= 1000.0");
    block.append("            return v");
    block.append("        bw = max(parse(result[0]), parse(result[1]))");
    block.append("        return {'bw_mbps': bw}");
    block.append("    except Exception as e:");
    block.append("        return {'bw_mbps': 0.0, 'error': str(e)}");
    block.append("");
    block.append("def _open_xterm(net, host_name):");
    block.append("    import os, subprocess");
    block.append("    h = net.get(host_name)");
    block.append("    env = os.environ.copy()");
    block.append("    if 'DISPLAY' not in env: env['DISPLAY'] = ':0'");
    block.append("    try:");
    block.append("        subprocess.Popen(['xterm', '-T', host_name,");
    block.append("                          '-bg', '#0F172A', '-fg', '#E2E8F0',");
    block.append("                          '-e', 'mnexec', '-a', str(h.pid), 'bash'], env=env)");
    block.append("        return {'ok': True}");
    block.append("    except Exception as e:");
    block.append("        return {'ok': False, 'error': str(e)}");
    block.append("");
    block.append("# ---------- симуляция отказа канала (#1) ----------");
    block.append("def _set_link_status(net, a, b, status):");
    block.append("    if not a or not b:");
    block.append("        return {'ok': False, 'error': 'нужны параметры from/to'}");
    block.append("    found = False");
    block.append("    for link in net.links:");
    block.append("        try:");
    block.append("            n1 = link.intf1.node.name; n2 = link.intf2.node.name");
    block.append("        except Exception:");
    block.append("            continue");
    block.append("        if set([n1, n2]) == set([a, b]):");
    block.append("            found = True; break");
    block.append("    if not found:");
    block.append("        return {'ok': False, 'error': 'канал %s-%s не найден' % (a, b)}");
    block.append("    try:");
    block.append("        net.configLinkStatus(a, b, status)");
    block.append("        print('[SNet] link %s-%s -> %s' % (a, b, status))");
    block.append("        return {'ok': True, 'from': a, 'to': b, 'status': status}");
    block.append("    except Exception as e:");
    block.append("        return {'ok': False, 'error': str(e)}");
    block.append("");
    block.append("# ---------- health Docker-контейнеров (#3) ----------");
    block.append("def _docker_health(net):");
    block.append("    import subprocess");
    block.append("    out = []");
    block.append("    for h in net.hosts:");
    block.append("        cname = 'mn.' + h.name");
    block.append("        try:");
    block.append("            r = subprocess.run(['docker', 'inspect', '--format',");
    block.append("                '{{.State.Status}}|{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}|{{.Config.Image}}',");
    block.append("                cname], capture_output=True, text=True, timeout=5)");
    block.append("        except Exception:");
    block.append("            continue");
    block.append("        if r.returncode != 0:");
    block.append("            continue  # не docker-узел");
    block.append("        parts = (r.stdout or '').strip().split('|')");
    block.append("        status = parts[0] if len(parts) > 0 else 'unknown'");
    block.append("        health = parts[1] if len(parts) > 1 else 'none'");
    block.append("        image = parts[2] if len(parts) > 2 else ''");
    block.append("        healthy = (health == 'healthy') or (health == 'none' and status == 'running')");
    block.append("        out.append({'name': h.name, 'image': image, 'status': status,");
    block.append("                    'health': health, 'healthy': healthy})");
    block.append("    return {'containers': out}");
    block.append("");
    block.append("def start_http_server(net, port):");
    block.append("    \"\"\"Биндимся на 127.0.0.1:port. Если занят — пробуем port+1..+10.");
    block.append("    Возвращает (server, bound_port). При неудаче — (None, 0).\"\"\"");
    block.append("    class Handler(BaseHTTPRequestHandler):");
    block.append("        def log_message(self, fmt, *args):");
    block.append("            return");
    block.append("        def _send(self, code, payload):");
    block.append("            body = json.dumps(payload).encode('utf-8')");
    block.append("            self.send_response(code)");
    block.append("            self.send_header('Content-Type', 'application/json; charset=utf-8')");
    block.append("            self.send_header('Content-Length', str(len(body)))");
    block.append("            self.send_header('Access-Control-Allow-Origin', '*')");
    block.append("            self.end_headers()");
    block.append("            self.wfile.write(body)");
    block.append("        def do_GET(self):");
    block.append("            try:");
    block.append("                qs = ''");
    block.append("                if '?' in self.path:");
    block.append("                    qs = self.path.split('?', 1)[1]");
    block.append("                path = self.path.split('?')[0].strip('/').split('/')");
    block.append("                if path == ['health'] or path == ['']:");
    block.append("                    return self._send(200, {'ok': True, 'app': 'SNet-Mininet'})");
    block.append("                if path == ['hosts']:");
    block.append("                    return self._send(200, {h.name: h.IP() for h in net.hosts})");
    block.append("                if path == ['pingall']:");
    block.append("                    return self._send(200, do_pingall(net))");
    block.append("                if len(path) == 3 and path[0] == 'ping':");
    block.append("                    return self._send(200, _ping_one(net, path[1], path[2]))");
    block.append("                if len(path) == 3 and path[0] == 'iperf':");
    block.append("                    return self._send(200, _iperf_pair(net, path[1], path[2]))");
    block.append("                if len(path) == 2 and path[0] == 'xterm':");
    block.append("                    return self._send(200, _open_xterm(net, path[1]))");
    block.append("                if path == ['traffic_events']:");
    block.append("                    since = 0.0");
    block.append("                    m = re.search(r'since=([\\d\\.]+)', qs)");
    block.append("                    if m: since = float(m.group(1))");
    block.append("                    return self._send(200, {'events': drain_events(since)})");
    block.append("                if path == ['docker', 'health']:");
    block.append("                    return self._send(200, _docker_health(net))");
    block.append("                if path == ['shutdown']:");
    block.append("                    self._send(200, {'ok': True})");
    block.append("                    import os, signal");
    block.append("                    os.kill(os.getpid(), signal.SIGINT)");
    block.append("                    return");
    block.append("                self._send(404, {'error': 'not found'})");
    block.append("            except Exception as e:");
    block.append("                self._send(500, {'error': str(e)})");
    block.append("        def do_POST(self):");
    block.append("            try:");
    block.append("                length = int(self.headers.get('Content-Length', 0) or 0)");
    block.append("                raw = self.rfile.read(length) if length else b''");
    block.append("                body = json.loads(raw.decode('utf-8')) if raw else {}");
    block.append("                path = self.path.split('?')[0].strip('/').split('/')");
    block.append("                if path == ['link', 'down']:");
    block.append("                    return self._send(200, _set_link_status(net, body.get('from'), body.get('to'), 'down'))");
    block.append("                if path == ['link', 'up']:");
    block.append("                    return self._send(200, _set_link_status(net, body.get('from'), body.get('to'), 'up'))");
    block.append("                self._send(404, {'error': 'not found'})");
    block.append("            except Exception as e:");
    block.append("                self._send(500, {'error': str(e)})");
    block.append("    last_err = None");
    block.append("    for p in range(port, port + 11):");
    block.append("        try:");
    block.append("            server = ThreadingHTTPServer(('127.0.0.1', p), Handler)");
    block.append("            t = threading.Thread(target=server.serve_forever, daemon=True)");
    block.append("            t.start()");
    block.append("            return server, p");
    block.append("        except OSError as e:");
    block.append("            last_err = e");
    block.append("            continue");
    block.append("    print('[SNet] !! Не удалось занять ни один порт в диапазоне %d..%d: %s' % (port, port + 10, last_err))");
    block.append("    return None, 0");
    block.append("");
    block.append("# ---------- entry point ----------");
    block.append("if __name__ == '__main__':");
    block.append("    parser = argparse.ArgumentParser(description='SDN Topology Mininet runner')");
    block.append("    parser.add_argument('--http', type=int, default=0, help='HTTP API port (0 = disabled)')");
    block.append("    parser.add_argument('--pingall', action='store_true', help='run pingAll and exit')");
    block.append("    args = parser.parse_args()");
    block.append("    setLogLevel('info')");
    block.append("    topology(http_port=args.http, run_pingall=args.pingall)");
    return block.join("\n").append("\n");
}

QString MininetScriptBuilder::buildMininetScript()
{

    startingData.append("net.build()");

    QString script;

    script.append(buildIncludeBlock());
    script.append(buildFunctionBeginBlock());

    script.append(buildSdnControllersBlock());
    script.append(buildHostsBlock());
    script.append(buildDockerNodesBlock());
    script.append(buildSwitchesBlock());
    script.append(buildLinksBlock());
    script.append(buildStartingBlock());

    script.append(buildFunctionEndBlock());
    script.append(buildStartupBlock());
    return script;
}
