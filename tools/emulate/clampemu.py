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
CRT_LOG = 0x1805C1680


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
        uc.mem_map(HEAP, 0x8000000)
        uc.mem_map(SENTINEL, 0x10000)
        uc.mem_write(SENTINEL, b"\xf4")
        self.heap = HEAP
        self.sizes = {}
        # Each import gets its own `ret` stub. A code hook emulates the Win32
        # heap calls the CRT allocator needs; every other import returns 0.
        self.stubs = {}
        n = 0
        for d in pe.DIRECTORY_ENTRY_IMPORT:
            for imp in d.imports:
                stub = SENTINEL + 0x100 + 8 * n
                n += 1
                uc.mem_write(stub, b"\xc3")
                uc.mem_write(imp.address, struct.pack("<Q", stub))
                self.stubs[stub] = (imp.name or b"?").decode()
        self.seen = set()
        uc.hook_add(UC_HOOK_CODE, self._import, begin=SENTINEL + 0x100, end=SENTINEL + 0x100 + 8 * n)
        uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._unmapped)
        uc.hook_add(UC_HOOK_CODE, self._crt_log, begin=CRT_LOG, end=CRT_LOG)

    def _crt_log(self, uc, addr, size, data):
        """log(+-0) is -inf. The CRT reports it through RaiseException, which needs Windows SEH."""
        x = uc.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFFFFFFFFFF
        if x & 0x7FFFFFFFFFFFFFFF == 0:
            uc.reg_write(UC_X86_REG_XMM0, 0xFFF0000000000000)
            rsp = uc.reg_read(UC_X86_REG_RSP)
            uc.reg_write(UC_X86_REG_RIP, struct.unpack("<Q", uc.mem_read(rsp, 8))[0])
            uc.reg_write(UC_X86_REG_RSP, rsp + 8)

    def _import(self, uc, addr, size, data):
        name = self.stubs.get(addr)
        rcx, rdx, r8, r9 = (uc.reg_read(r) for r in (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9))
        ret = 0
        if name == "GetProcessHeap":
            ret = 1
        elif name == "HeapAlloc":
            ret = self.alloc(r8)
        elif name == "HeapReAlloc":
            ret = self.alloc(r9)
            old = self.sizes.get(r8, 0)
            if old:
                uc.mem_write(ret, bytes(uc.mem_read(r8, min(old, r9))))
        elif name == "HeapSize":
            ret = self.sizes.get(r8, 0)
        elif name in ("HeapFree", "EnterCriticalSection", "LeaveCriticalSection"):
            ret = 1
        elif name not in self.seen:
            self.seen.add(name)
            print(f"[import stub] {name} -> 0", file=sys.stderr)
        uc.reg_write(UC_X86_REG_RAX, ret)

    def _unmapped(self, uc, access, addr, size, value, data):
        rip = uc.reg_read(UC_X86_REG_RIP)
        print(f"unmapped access {access} at {addr:#x} from rip {rip:#x}", file=sys.stderr)
        return False

    def alloc(self, n):
        p = self.heap
        self.heap += (max(n, 1) + 0xF) & ~0xF
        self.uc.mem_write(p, b"\0" * n)
        self.sizes[p] = n
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


OVERSAMPLER_CTOR = 0x18054AC90
METER_UPDATE = 0x180549360


class Plugin:
    """The original Clamp DSP state, driven through its real ProcessBlock.

    Only the iPlug2 base and the GUI are missing: parameters are a plain
    IParam* array, and the GUI meter update is patched to return at once.
    """

    def __init__(self, emu, sample_rate=48000.0, block=512):
        self.e = e = emu
        e.uc.mem_write(METER_UPDATE, b"\xc3")
        self.base = b = e.alloc(0x1000)
        self.params = []
        arr = e.alloc(8 * 8)
        for i in range(8):
            p = e.alloc(0x40)
            self.params.append(p)
            e.wq(arr + 8 * i, p)
        e.wq(b + 8, arr)
        e.uc.mem_write(b + 0x14, struct.pack("<I", 64))
        e.wd(b + 0x4C8, sample_rate)
        e.uc.mem_write(b + 0x4D0, struct.pack("<i", block))
        e.wd(b + 0x870, 24.0)
        for obj, vt in ((b + 0x878, VT_NAIVE), (b + 0x8D0, VT_CLIPPER)):
            e.wq(obj, vt)
            for off in (0x30, 0x38, 0x40, 0x48, 0x50):
                e.wd(obj + off, 1.0)
        e.uc.mem_write(b + 0x938, struct.pack("<i", -1))
        e.call(OVERSAMPLER_CTOR, (b + 0x938, 0, 0, 0))
        e.uc.mem_write(b + 0xBE0, b"\x01")
        self.block = block

    def set(self, idx, value):
        self.e.wd(self.params[idx] + 8, float(value))

    def reset_oversampling(self):
        """What the original does after an oversampling change (flag at 0xbe0)."""
        self.e.uc.mem_write(self.base + 0xBE0, b"\x01")

    def process(self, left, right):
        e = self.e
        n = len(left)
        out_l, out_r = [], []
        bufs = [e.alloc(8 * self.block) for _ in range(4)]
        ins, outs = e.alloc(16), e.alloc(16)
        e.wq(ins, bufs[0]); e.wq(ins + 8, bufs[1])
        e.wq(outs, bufs[2]); e.wq(outs + 8, bufs[3])
        for s in range(0, n, self.block):
            m = min(self.block, n - s)
            e.uc.mem_write(bufs[0], struct.pack(f"<{m}d", *left[s:s + m]))
            e.uc.mem_write(bufs[1], struct.pack(f"<{m}d", *right[s:s + m]))
            e.call(PROCESS_BLOCK, (self.base + 0x4B0, ins, outs, m))
            out_l += struct.unpack(f"<{m}d", e.uc.mem_read(bufs[2], 8 * m))
            out_r += struct.unpack(f"<{m}d", e.uc.mem_read(bufs[3], 8 * m))
        return out_l, out_r
