import random, subprocess, sys, math
sys.path.insert(0, '.')
from clampemu import Emu
e = Emu(sys.argv[1])
random.seed(1)
cases = []
for kind in 'AB':
    for mode in range(6):
        for c in [0, -0.5, -3, -6, -12, -24, -36, -48]:
            for sh in [0, 0.25, 0.5, 0.75, 1]:
                for _ in range(6):
                    ing = 10 ** (random.uniform(-24, 24) / 20); outg = 10 ** (random.uniform(-24, 24) / 20)
                    l = random.choice([1e-12, random.uniform(-2, 2), random.uniform(-0.05, 0.05)]); r = random.uniform(-2, 2)
                    cases.append((kind, mode, ing, outg, c, sh, l, r))
inp = "\n".join(" ".join(x if isinstance(x, str) else repr(x) for x in cs) for cs in cases)
port = subprocess.run([sys.argv[2] if len(sys.argv) > 2 else './port'], input=inp, capture_output=True, text=True).stdout.split("\n")
worst = (0, None)
bad = 0
for cs, line in zip(cases, port):
    kind, mode, ing, outg, c, sh, l, r = cs
    o = e.shaper(kind, mode, ing, outg, c, sh)
    ol, orr = e.process(o, l, r)
    pl, pr = map(float, line.split())
    d = max(abs(ol - pl), abs(orr - pr))
    if d > 1e-9: bad += 1
    if d > worst[0]: worst = (d, cs, (ol, orr), (pl, pr))
print(f"cases={len(cases)} mismatches(>1e-9)={bad} worst={worst}")
