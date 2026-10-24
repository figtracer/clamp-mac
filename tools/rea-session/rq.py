# Send one request to readsrv: rq.py SOCK METHOD [JSON_PARAMS] or tool call: rq.py SOCK call NAME JSON_ARGS.
import json, socket, sys
sock = sys.argv[1]
if sys.argv[2] == "call":
    req = {"method": "tools/call", "params": {"name": sys.argv[3], "arguments": json.loads(sys.argv[4] if len(sys.argv) > 4 else "{}")}}
else:
    req = {"method": sys.argv[2], "params": json.loads(sys.argv[3] if len(sys.argv) > 3 else "{}")}
c = socket.socket(socket.AF_UNIX); c.connect(sock); c.sendall((json.dumps(req) + "\n").encode()); c.shutdown(socket.SHUT_WR)
buf = b""
while True:
    d = c.recv(1 << 20)
    if not d: break
    buf += d
r = json.loads(buf)
if "result" in r and "content" in r["result"]:
    for item in r["result"]["content"]:
        print(item.get("text", item))
else:
    print(json.dumps(r))
