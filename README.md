# clamp-mac

An unofficial, native macOS (Apple Silicon and Intel) port of
[Outobugi](https://outobugi.com)'s free Clamp clipper. Outobugi ships it
for Windows only. The DSP was reconstructed from the Windows VST3 with
[REA](https://github.com/morluto/rea) and Ghidra, then rebuilt on
[iPlug2](https://github.com/iPlug2/iPlug2) as a VST3 and an AU.

This project is not affiliated with Outobugi. All credit for the plugin's
design goes to them. If you are on Windows, use the original.

## Install a release

Download `Clamp-Port-<version>-macOS-Installer.pkg` from
[Releases](../../releases) and run it. It installs both plugins:

- VST3: `/Library/Audio/Plug-Ins/VST3/Clamp.vst3`
- AU: `/Library/Audio/Plug-Ins/Components/Clamp.component`

The binaries are universal (Apple Silicon and Intel) and need macOS 11 or
later. The installer is not signed with an Apple Developer ID, so macOS
blocks it the first time. Open **System Settings → Privacy & Security**,
click **Open Anyway** next to the blocked installer, and run it again.
Then rescan plugins in your DAW. It is listed as **Clamp Port** by
**clamp-mac**.

### Manual install from the zips

A macOS `.vst3` or `.component` is a bundle: a folder with the binary in
`Contents/MacOS` and fonts in `Contents/Resources`. If no DAW is installed,
Finder may show it as a plain folder. Move the whole folder:

1. Move `Clamp.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and
   `Clamp.component` to `~/Library/Audio/Plug-Ins/Components/`.
2. Remove the download quarantine flag, or macOS blocks them:

   ```bash
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Clamp.vst3 ~/Library/Audio/Plug-Ins/Components/Clamp.component
   ```

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
- `tools/emulate/`: runs the original DLL's DSP under an x86-64 emulator and compares it with the port.
- `scripts/package.sh`: builds universal binaries, the installer, and the
  release zips.
- `tests/`: checks the shapers against the recovered formulas.

```bash
c++ -std=c++17 tests/waveshapers_test.cpp -o /tmp/wtest && /tmp/wtest
```
