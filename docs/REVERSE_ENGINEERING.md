# Reverse-engineering notes

Target: `Clamp.vst3` v2.2.0 by Outobugi, a Windows x64 PE DLL (SHA-256
`ddd2314c7d4f868570a36d1f8c3959cc5e62a04eb70811364a3bd7fcbd74e71e`).
Built with iPlug2 and VST3 SDK 3.7.11, MSVC. No PDB ships; RTTI names
remain. All addresses below use the image base `0x180000000`.

The original binary is not in this repo. Get it from https://outobugi.com.

## Tooling

- [REA](https://github.com/morluto/rea) 6.7.0 (`rea-agents`) with the
  Ghidra provider.
- REA 6.7.0 accepts only Ghidra 12.1.x. The official 12.1.4 zip has no
  macOS arm64 natives, so the `decompile` and `sleigh` binaries under
  `Ghidra/Features/*/os/mac_arm_64` were copied from Homebrew's Ghidra
  12.2. This worked for every query below.
- REA needs Node 22, 24, or 26+ (not 25), and a full JDK 25+ in
  `JAVA_HOME`.
- Each `rea` CLI call imports the DLL into Ghidra again, which takes
  about 4.5 minutes. `tools/rea-session/` keeps one `rea --mcp` session
  open and exposes it on a Unix socket:

  ```bash
  python3 tools/rea-session/readsrv.py /tmp/rea.sock "$(which rea)" &
  python3 tools/rea-session/rq.py /tmp/rea.sock call open_binary '{"path":"/abs/Clamp.dll","provider_id":"ghidra"}'
  python3 tools/rea-session/rq.py /tmp/rea.sock call procedure_pseudo_code '{"procedure":"0x180548b40"}'
  ```

  Keep the socket path short. macOS limits `AF_UNIX` paths to 104 bytes.

## How the code was found

1. `search_strings` / `list_strings`: plugin strings sit together near
   `0x1806c5c00` (`Outobugi`, `Clipper algorithm.`, `CEILING`, `SHAPE`,
   `TYPE A`, ...).
2. `xrefs` on `CEILING` led to the plugin constructor `0x1805463f0`.
3. `find_xrefs_to_name` on `Waveshapers::*::vftable` gave the shaper
   vtables. Slot 0 of each vtable is the per-sample shape function.
4. `procedure_callers` on `exp` (`0x1805bc990`) led to `ProcessBlock`
   (`0x180548b40`). It is called through a `this - 0x4b0` thunk.
5. `ProcessBlock` hands a lambda to the oversampler. The lambda's
   `std::function` vtable `0x1806c5a68` has `_Do_call` at slot 2
   (`0x18055be60`).

## Functions

| Address | Role |
| --- | --- |
| `0x1805463f0` | `Clamp::Clamp`: parameter setup and member init. |
| `0x180548b40` | `Clamp::ProcessBlock`: reads parameters and sets up both shapers. |
| `0x180548eb0` | Oversampling change: sets the factor and meter timing. |
| `0x18055be60` | Lambda: runs the shaper selected by TYPE on each sample, then updates the meters. |
| `0x180542380` | `Waveshaper::Process(L, R)`: channel mode routing and gain staging. |
| `0x1805425b0` | `NaiveClipper` shape function (TYPE A). |
| `0x180542660` | `Clipper` shape function (TYPE B). |
| `0x1805494b0` | iPlug2 `OverSampler<double>::ProcessBlock` (HIIR, 12/4/3/2 coefficient stages). |
| `0x1805c0630` | CRT `tanh`. |

## Parameters

| # | Name | Kind | Range / options | Default |
| --- | --- | --- | --- | --- |
| 0 | BYPASS | bool | OFF, ON | OFF |
| 1 | OVERSAMPLING | enum | OFF, 2X, 4X, 8X, 16X | OFF |
| 2 | IN | dB | -24 to 24, step 0.5 | 0 |
| 3 | OUT | dB | -24 to 24, step 0.5 | 0 |
| 4 | CEILING | dB | -48 to 0, step 0.5 | 0 |
| 5 | SHAPE | linear | 0 to 1, step 0.01 | 1 |
| 6 | TYPE | enum | TYPE A, TYPE B | TYPE A |
| 7 | MODE | enum | LR, L, R, MS, M, S | LR |

## DSP

For each sample, both channels go through `Waveshaper::Process`:

- LR, L, R: process the left and/or right channel.
- MS, M, S: `M = (L + R) / 2`, `S = (L - R) / 2`; process M and/or S,
  then `L = M + S`, `R = M - S`.
- Each processed component gets `x *= in`, then `shape(x)`, then
  `x *= out`. Components that are not processed pass through
  unchanged, without any gain.
- A dry/wet stage follows. Its mix is always 1.

TYPE A (`NaiveClipper`), with `c = 10^(ceiling / 20)`:

```text
soft = tanh(x / c) * c
hard = clamp(x, -c, c)
y    = (hard - soft) * shape + soft
```

TYPE B (`Clipper`): an infinite-ratio gain computer in dB, with
`T = ceiling` and knee width `W = (1 - shape) * 24`:

```text
xdB = 20 log10 |x|
if W > 0 and T - W/2 < xdB < T + W/2:
    g = -(xdB - (T - W/2))^2 / (2W)
else:
    g = min(T - xdB, 0)
y = x * 10^(g / 20)
```

Gains are read once per block, with no smoothing. Bypass copies input to
output.

## How close the port is

### Audio: matches, except libm rounding

`tools/emulate/block_diff.py` runs the original `ProcessBlock`, with its
oversampler, under emulation. It runs `Clamp/ClampDSP.h`, the code the
plugin uses, on the same scenarios: 60 runs of sines, sweeps, noise and
transients, with parameter, oversampling and bypass changes between
blocks of 7 to 512 samples. Of 117,768 output samples, 91.35% are
bit-identical. The largest difference is 1.07e-14 (about -280 dB).

The rest comes from the C math library. The DLL links Microsoft's CRT
`tanh`, `log` and `exp`. These differ from macOS libm in the last bit for
2.4%, 0.04% and 0.5% of inputs. To remove it, the port would have to
carry a copy of Microsoft's math code. It does not.

Checked to match exactly:

- Shaper formulas, gain staging, channel modes, and the dry/wet stage.
- Parameter-to-DSP mapping, including the truncated dB constant
  `0.1151292546497` and `20 / ln(10) = 8.685889638065037`.
- All 21 HIIR oversampler coefficients, bit for bit.
- Oversampler reset timing: only at construction and when OVERSAMPLING
  changes (`OnParamChange`, `0x180549140`), never on host reset.
- Mono: both always process two channels. An unconnected channel is
  silence.
- Parameter names, ranges, defaults, steps, units and types. BYPASS is
  `InitBool`, which the compiler inlined.
- No latency is reported, and parameters are not smoothed.

One deliberate difference: when the host block grows past the size the
oversampler was reset with, the original writes past its buffers. The
port resets the oversampler for the new size instead.

### UI: same layout, not the same implementation

The original editor is built from these classes: `Header`, `Footer`,
`BasePanel`, `GlassPanel`, `FramePanel`, `IndentDot`, `DynamicsDisplay`,
`ThresholdSlider`, `BigKnob`, `KnobBase`, `Slider`, `SimpleSpinBox`,
`ShapeDisplay`, `SimpleValueBox`, `ValueDisplay`, `DropMenu`,
`ParamMenu`, `SVGButton`, `ColorMenu`, `TooltipDisplay`,
`PresetManager`, `PresetList`, and `PresetListItem`.

The port draws a similar layout with its own code. These parts are not
ported:

- The preset browser (`PresetManager`), which saves files to
  `%APPDATA%/Clamp/Presets`.
- The accent colour menu (`ColorMenu`) and its `Settings.ini`. This is
  the red circle in the header. The port uses that button to freeze the
  level display.
- Oversampling as a drop-down menu (`DropMenu`). The port cycles
  through the values.
- `TooltipDisplay`. The original shows three tooltip texts: "Sets the
  input gain of chosen channel(s)", "Sets the output gain of chosen
  channel(s)", and "Clipper algorithm.".
- How exactly the display scrolls and scales. The port shows the level
  after IN gain and before OUT gain.

The plugin name and IDs are also different, on purpose.
