# Run Clamp.dll's own DSP functions under Unicorn (x86-64), with no Windows loader.
import struct
import sys

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_MEM_UNMAPPED, UC_HOOK_CODE
from unicorn.x86_const import *

BASE = 0x180000000
STACK = 0x7FF000000
HEAP = 0x10000000
SENTINEL = 0xDEAD0000

PROCESS = 0x180542380        # Waveshaper::Process(this, double* L, double* R, double wet)
PROCESS_BLOCK = 0x180548B40  # Clamp::ProcessBlock thunk (this + 0x4b0).
OVERSAMPLER_CALL = 0x1805494B0
VT_CLIPPER = 0x1806C21E8
VT_NAIVE = 0x1806C2710


class Emu:
    def __init__(self, dll):
        pe = pefile.PE(dll)
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_64)
        uc.mem_map(BASE, size)
        uc.mem_write(BASE, pe.header)
        for s in pe.sections:
            data = s.get_data()
            uc.mem_write(BASE + s.VirtualAddress, data[: max(s.Misc_VirtualSize, 0) or len(data)])
        uc.mem_map(STACK, 0x100000)
        uc.mem_map(HEAP, 0x1000000)
        uc.mem_map(SENTINEL, 0x1000)
        uc.mem_write(SENTINEL, b"\xf4")
        self.heap = HEAP
        # Imports are never resolved; point every IAT slot at a stub that returns 0.
        stub = SENTINEL + 0x10
        uc.mem_write(stub, b"\x31\xc0\xc3")
        for d in pe.DIRECTORY_ENTRY_IMPORT:
            for imp in d.imports:
                uc.mem_write(imp.address, struct.pack("<Q", stub))
        uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._unmapped)

    def _unmapped(self, uc, access, addr, size, value, data):
        rip = uc.reg_read(UC_X86_REG_RIP)
        print(f"unmapped access {access} at {addr:#x} from rip {rip:#x}", file=sys.stderr)
        return False

    def alloc(self, n):
        p = self.heap
        self.heap += (n + 0xF) & ~0xF
        self.uc.mem_write(p, b"\0" * n)
        return p

    def wq(self, a, v):
        self.uc.mem_write(a, struct.pack("<Q", v))

    def wd(self, a, v):
        self.uc.mem_write(a, struct.pack("<d", v))

    def rd(self, a):
        return struct.unpack("<d", self.uc.mem_read(a, 8))[0]

    def call(self, fn, args=(), xmm=None, stop_at=None):
        uc = self.uc
        sp = STACK + 0xF0000
        sp -= 8
        self.wq(sp, SENTINEL)
        uc.reg_write(UC_X86_REG_RSP, sp)
        for reg, v in zip((UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9), args):
            uc.reg_write(reg, v)
        for i, v in (xmm or {}).items():
            uc.reg_write(getattr(sys.modules["unicorn.x86_const"], f"UC_X86_REG_XMM{i}"),
                         struct.unpack("<Q", struct.pack("<d", v))[0])
        uc.emu_start(fn, stop_at if stop_at is not None else SENTINEL, count=50_000_000)

    def shaper(self, kind, mode, in_gain, out_gain, ceiling_db, shape):
        """Builds a shaper object laid out exactly as ProcessBlock fills it."""
        o = self.alloc(0x80)
        self.wq(o, VT_NAIVE if kind == "A" else VT_CLIPPER)
        self.uc.mem_write(o + 8, struct.pack("<i", mode))
        self.wd(o + 0x30, out_gain)
        self.wd(o + 0x38, in_gain)
        self.wd(o + 0x40, 1.0)
        if kind == "A":
            self.wd(o + 0x48, 10 ** (ceiling_db / 20))
            self.wd(o + 0x50, shape)
        else:
            knee = (1 - shape) * 24.0
            self.wd(o + 0x48, ceiling_db)
            self.wd(o + 0x50, ceiling_db - knee / 2)
            self.wd(o + 0x58, ceiling_db + knee / 2)
            self.wd(o + 0x60, knee)
        return o

    def process(self, obj, l, r):
        pl, pr = self.alloc(8), self.alloc(8)
        self.wd(pl, l)
        self.wd(pr, r)
        self.call(PROCESS, (obj, pl, pr), xmm={3: 1.0})
        return self.rd(pl), self.rd(pr)

    def process_block_fields(self, params):
        """Runs the real ProcessBlock up to the oversampler call; returns the shaper fields it wrote."""
        base = self.alloc(0x2000)
        this = base + 0x4B0
        arr = self.alloc(8 * len(params))
        for i, v in enumerate(params):
            p = self.alloc(0x40)
            self.wd(p + 8, v)
            self.wq(arr + 8 * i, p)
        self.wq(base + 8, arr)
        self.uc.mem_write(base + 0x14, struct.pack("<I", 8 * len(params)))
        self.wd(this + 0x3C0, 24.0)
        ins = self.alloc(16)
        outs = self.alloc(16)
        self.call(PROCESS_BLOCK, (this, ins, outs, 0), stop_at=OVERSAMPLER_CALL)
        naive, clip = base + 0x878, base + 0x8D0
        return {
            "A.out": self.rd(naive + 0x30), "A.in": self.rd(naive + 0x38),
            "A.mode": struct.unpack("<i", self.uc.mem_read(naive + 8, 4))[0],
            "A.ceiling_lin": self.rd(naive + 0x48), "A.shape": self.rd(naive + 0x50),
            "B.out": self.rd(clip + 0x30), "B.in": self.rd(clip + 0x38),
            "B.T": self.rd(clip + 0x48), "B.lo": self.rd(clip + 0x50),
            "B.hi": self.rd(clip + 0x58), "B.knee": self.rd(clip + 0x60),
        }
