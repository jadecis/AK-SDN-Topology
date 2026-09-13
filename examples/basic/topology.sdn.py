#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Сгенерировано SDN Topology (SNet)

import argparse
import json
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer

from mininet.net import Mininet
from mininet.node import Controller, RemoteController, OVSKernelSwitch
from mininet.cli import CLI
from mininet.log import setLogLevel
from mininet.link import TCLink

def topology(http_port=0, run_pingall=False):
	net = Mininet(controller=RemoteController, link=TCLink, switch=OVSKernelSwitch)

	c0 = net.addController('c0', controller=RemoteController, ip='127.0.0.1', port=6653)

	h1 = net.addHost('h1', mac='00:00:00:00:00:01', ip='10.0.0.1')
	h2 = net.addHost('h2', mac='00:00:00:00:00:02', ip='10.0.0.2')

	s1 = net.addSwitch('s1')
	s2 = net.addSwitch('s2')
	s3 = net.addSwitch('s3')
	s4 = net.addSwitch('s4')

	net.addLink(s3, s1, 1, 1, bw=1, delay='1ms', loss=0)
	net.addLink(s2, s3, 1, 2, bw=1, delay='1ms', loss=0)
	net.addLink(s4, s2, 1, 2, bw=1, delay='1ms', loss=0)
	net.addLink(s1, s4, 2, 2, bw=1, delay='1ms', loss=0)
	net.addLink(s3, s4, 3, 3, bw=1, delay='1ms', loss=0)
	net.addLink(s2, s1, 3, 3, bw=1, delay='1ms', loss=0)
	net.addLink(s3, h1, 4, 1, bw=1, delay='1ms', loss=0)
	net.addLink(h2, s4, 1, 4, bw=1, delay='1ms', loss=0)

	c0.start()
	s1.start([c0])
	s2.start([c0])
	s3.start([c0])
	s4.start([c0])
	net.build()

	
	if http_port:
		server = start_http_server(net, http_port)
		print('[SNet] HTTP server listening on 127.0.0.1:%d' % http_port)
	else:
		server = None
	
	if run_pingall:
		result = do_pingall(net)
		print('SNET_PINGALL_JSON:' + json.dumps(result))
		net.stop()
		if server: server.shutdown()
		return
	
	CLI(net)
	if server: server.shutdown()
	net.stop()


# ---------- HTTP-сервер для GUI (SNet) ----------
def _ping_one(net, h1_name, h2_name):
	h1 = net.get(h1_name); h2 = net.get(h2_name)
	ip2 = h2.IP()
	raw = h1.cmd('ping -c 4 -W 1 ' + ip2)
	loss = 100.0; rtt = -1.0
	m = re.search(r'(\d+)% packet loss', raw)
	if m: loss = float(m.group(1))
	m = re.search(r'rtt[^=]*= [\d\.]+/([\d\.]+)/', raw)
	if m: rtt = float(m.group(1))
	return {'rtt_ms': rtt, 'loss': loss, 'raw': raw[-2000:]}

def do_pingall(net):
	hosts = net.hosts
	matrix = []
	for h1 in hosts:
		for h2 in hosts:
			if h1 is h2: continue
			res = _ping_one(net, h1.name, h2.name)
			matrix.append({'src': h1.name, 'dst': h2.name, 'rtt_ms': res['rtt_ms'], 'loss': res['loss']})
	return {'matrix': matrix}

def _iperf_pair(net, h1_name, h2_name):
	h1 = net.get(h1_name); h2 = net.get(h2_name)
	try:
		result = net.iperf((h1, h2), seconds=2)
		# net.iperf returns [server_bw, client_bw] as strings like '12.3 Mbits/sec'
		def parse(s):
			m = re.search(r'([\d\.]+)\s*([KMG])?bits', str(s))
			if not m: return 0.0
			v = float(m.group(1)); unit = m.group(2)
			if unit == 'K': v /= 1000.0
			elif unit == 'G': v *= 1000.0
			return v
		bw = max(parse(result[0]), parse(result[1]))
		return {'bw_mbps': bw}
	except Exception as e:
		return {'bw_mbps': 0.0, 'error': str(e)}

def start_http_server(net, port):
	class Handler(BaseHTTPRequestHandler):
		def log_message(self, fmt, *args):
			return
		def _send(self, code, payload):
			body = json.dumps(payload).encode('utf-8')
			self.send_response(code)
			self.send_header('Content-Type', 'application/json; charset=utf-8')
			self.send_header('Content-Length', str(len(body)))
			self.send_header('Access-Control-Allow-Origin', '*')
			self.end_headers()
			self.wfile.write(body)
		def do_GET(self):
			try:
				path = self.path.split('?')[0].strip('/').split('/')
				if path == ['health'] or path == ['']:
					return self._send(200, {'ok': True, 'app': 'SNet-Mininet'})
				if path == ['hosts']:
					return self._send(200, {h.name: h.IP() for h in net.hosts})
				if path == ['pingall']:
					return self._send(200, do_pingall(net))
				if len(path) == 3 and path[0] == 'ping':
					res = _ping_one(net, path[1], path[2])
					return self._send(200, res)
				if len(path) == 3 and path[0] == 'iperf':
					res = _iperf_pair(net, path[1], path[2])
					return self._send(200, res)
				self._send(404, {'error': 'not found'})
			except Exception as e:
				self._send(500, {'error': str(e)})
	server = HTTPServer(('127.0.0.1', port), Handler)
	t = threading.Thread(target=server.serve_forever, daemon=True)
	t.start()
	return server

# ---------- entry point ----------
if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='SDN Topology Mininet runner')
	parser.add_argument('--http', type=int, default=0, help='HTTP API port (0 = disabled)')
	parser.add_argument('--pingall', action='store_true', help='run pingAll and exit')
	args = parser.parse_args()
	setLogLevel('info')
	topology(http_port=args.http, run_pingall=args.pingall)
