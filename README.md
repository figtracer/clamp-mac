# clamp-mac

An unofficial, native macOS (Apple Silicon and Intel) port of
[Outobugi](https://outobugi.com)'s free Clamp clipper. Outobugi ships it
for Windows only. The DSP was reconstructed from the Windows VST3 with
[REA](https://github.com/morluto/rea) and Ghidra, then rebuilt on
[iPlug2](https://github.com/iPlug2/iPlug2) as a VST3 and an AU.

This project is not affiliated with Outobugi. All credit for the plugin's
design goes to them. If you are on Windows, use the original.

## Build

You need Xcode command-line tools, CMake, and git.

```bash
./scripts/build.sh
```

The script fetches iPlug2 (submodule) and the VST3 SDK, builds, and
installs `Clamp.vst3` and `Clamp.component` into
`~/Library/Audio/Plug-Ins`. The build is ad-hoc signed. That is enough
for your own machine.

## Layout

- `Clamp/`: plugin source. The DSP is in `Waveshapers.h`.
- `docs/REVERSE_ENGINEERING.md`: what was recovered, where it is in the
  binary, and how it was found.
- `tools/rea-session/`: keeps one REA MCP session open, so queries do not
  re-import the binary each time.
- `tests/`: checks the shapers against the recovered formulas.

```bash
c++ -std=c++17 tests/waveshapers_test.cpp -o /tmp/wtest && /tmp/wtest
```
