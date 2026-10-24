"""Runs the original ProcessBlock (emulated) and the port's ClampDSP on the same
scenarios and compares every output sample bit for bit."""
import math
import random
import subprocess
import sys

sys.path.insert(0, __import__("os").path.dirname(__file__))
from clampemu import Emu, Plugin  # noqa: E402

DLL, PORT = sys.argv[1], sys.argv[2]
random.seed(7)


def signal(n, kind):
    out = []
    for i in range(n):
        t = i / 48000
        if kind == "sine":
            v = 1.4 * math.sin(2 * math.pi * 997 * t)
        elif kind == "sweep":
            v = 1.2 * math.sin(2 * math.pi * (50 + 9000 * t) * t)
        elif kind == "noise":
            v = random.gauss(0, 0.6)
        else:  # Transients.
            v = (2.0 if i % 700 < 3 else 0.0) + 0.05 * math.sin(2 * math.pi * 220 * t)
        out.append(v)
    return out


def random_params():
    return [0, random.randint(0, 4), random.uniform(-24, 24), random.uniform(-24, 24),
            random.choice([0, -0.5, -6, -12, random.uniform(-48, 0)]), random.choice([0, 1, random.random()]),
            random.randint(0, 1), random.randint(0, 5)]


scenarios = []
for k in range(60):
    steps, prev_os = [], None
    blocks = random.choice([[512] * 3, [256, 512, 100, 7], [64] * 6])
    kinds = ["sine", "sweep", "noise", "trans"]
    l = signal(sum(blocks), random.choice(kinds))
    r = signal(sum(blocks), random.choice(kinds))
    params = random_params()
    pos = 0
    for bi, n in enumerate(blocks):
        if bi == 0 or random.random() < 0.4:
            new = random_params() if bi else params
            if bi and random.random() < 0.15:
                new[0] = 1  # Bypass for a block.
            params = new
        steps.append(("params", list(params)))
        if prev_os is None or params[1] != prev_os:
            steps.append(("osreset",))
            prev_os = params[1]
        steps.append(("block", l[pos:pos + n], r[pos:pos + n]))
        pos += n
    scenarios.append(steps)

exact = total = 0
worst = 0.0
for si, steps in enumerate(scenarios):
    emu = Emu(DLL)
    orig = Plugin(emu, block=512)
    script = []
    o_out = []
    first = True
    for st in steps:
        if st[0] == "params":
            for i, v in enumerate(st[1]):
                orig.set(i, v)
            script.append("params " + " ".join(repr(v) for v in st[1]))
        elif st[0] == "osreset":
            if not first:
                orig.reset_oversampling()  # The constructor already armed the first one.
            script.append("osreset")
            first = False
        else:
            ol, orr = orig.process(st[1], st[2])
            o_out.append((ol, orr))
            script.append("block %d %s" % (len(st[1]), " ".join(f"{a!r} {b!r}" for a, b in zip(st[1], st[2]))))
    res = subprocess.run([PORT], input="\n".join(script) + "\n", capture_output=True, text=True).stdout.split("\n")
    for (ol, orr), line in zip(o_out, res):
        vals = [float.fromhex(v) for v in line.split()]
        for i in range(len(ol)):
            for a, b in ((ol[i], vals[2 * i]), (orr[i], vals[2 * i + 1])):
                total += 1
                if a == b or (math.isnan(a) and math.isnan(b)):
                    exact += 1
                else:
                    worst = max(worst, abs(a - b))
print(f"scenarios={len(scenarios)} samples={total} bit-exact={exact} ({100 * exact / total:.4f}%) max_abs_diff={worst:.3e}")
