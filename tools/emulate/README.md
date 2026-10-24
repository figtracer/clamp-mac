# Differential test against the original DLL

Runs the original Clamp.dll DSP functions under the Unicorn x86-64 emulator,
with no Windows loader, and compares them with the port's `Waveshapers.h`.

```bash
python3 -m venv .venv && .venv/bin/pip install unicorn pefile
c++ -std=c++17 -O2 port.cpp -o port
.venv/bin/python diff.py /abs/path/to/Clamp.dll ./port
```

`clampemu.py` maps the PE at its preferred base and stubs every import with
`return 0`. It can call `Waveshaper::Process` (`0x180542380`) on a shaper
object built in emulated memory, and it can run the real `ProcessBlock`
(`0x180548b40`) up to the oversampler call to read the shaper fields that it
derives from the parameters.

Exact-zero input to TYPE B is skipped. The CRT `log(0)` path raises a
floating-point exception through `RaiseException`, which needs real Windows
SEH. Silence gives 0 in both the original and the port.

Result at v1.0.0: 2880 cases, 0 mismatches above 1e-9.
