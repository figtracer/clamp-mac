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

`block_diff.py` runs the whole original `ProcessBlock`, oversampler
included, against the plugin's real audio code in `Clamp/ClampDSP.h`:

```bash
c++ -std=c++17 -O2 -ffp-contract=off -I../../iPlug2/IPlug -I../../iPlug2/IPlug/Extras -I../../iPlug2/WDL port_block.cpp -o port_block
.venv/bin/python block_diff.py /abs/path/to/Clamp.dll ./port_block
```

The emulator stands in for the Win32 heap calls the CRT allocator needs.
It returns -inf for `log(0)` directly, because the CRT's error path
needs Windows SEH. It also stubs out the GUI meter update.

Results at 1.2.0: `diff.py` finds 0 differences above 1e-9 in 2880
cases. `block_diff.py` finds 91.35% of 117,768 samples bit-identical,
with a largest difference of 1.07e-14. The remaining differences are
Microsoft CRT and macOS libm rounding in `tanh`, `log` and `exp`.
