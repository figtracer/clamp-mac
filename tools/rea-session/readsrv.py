# Long-lived rea MCP stdio session exposed on a unix socket.
import json, os, socket, subprocess, sys, threading
sock_path, rea = sys.argv[1], sys.argv[2]
p = subprocess.Popen([rea, "--mcp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=open(sock_path + ".stderr", "w"), text=True, bufsize=1)
lock = threading.Lock(); nid = [0]
def rpc(method, params, notify=False):
    with lock:
        msg = {"jsonrpc": "2.0", "method": method, "params": params}
        if not notify:
            nid[0] += 1; msg["id"] = nid[0]
        p.stdin.write(json.dumps(msg) + "\n"); p.stdin.flush()
        if notify: return None
        while True:
            line = p.stdout.readline()
            if not line: raise SystemExit("rea exited")
            r = json.loads(line)
            if r.get("id") == nid[0]: return r
rpc("initialize", {"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "cc", "version": "1"}})
rpc("notifications/initialized", {}, notify=True)
if os.path.exists(sock_path): os.unlink(sock_path)
s = socket.socket(socket.AF_UNIX); s.bind(sock_path); s.listen(4)
while True:
    c, _ = s.accept(); data = b""
    while not data.endswith(b"\n"):
        chunk = c.recv(65536)
        if not chunk: break
        data += chunk
    req = json.loads(data)
    r = rpc(req["method"], req.get("params", {}))
    c.sendall(json.dumps(r).encode()); c.close()
