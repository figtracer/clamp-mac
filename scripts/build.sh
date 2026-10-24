#!/usr/bin/env bash
# Fetch dependencies, then build and install the VST3 and AU for this Mac.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
sdk="$root/iPlug2/Dependencies/IPlug/VST3_SDK"

git -C "$root" submodule update --init --depth 1 iPlug2

if [ ! -d "$sdk/pluginterfaces/base" ]; then
  rm -rf "$sdk"
  git clone --depth 1 https://github.com/steinbergmedia/vst3sdk.git "$sdk"
  git -C "$sdk" submodule update --init --depth 1 pluginterfaces base public.sdk cmake
fi

# The Xcode generator can hang in try-compile; Makefiles work.
cmake -S "$root/Clamp" -B "$root/Clamp/build" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
cmake --build "$root/Clamp/build" -j"$(sysctl -n hw.ncpu)"

echo "Installed to ~/Library/Audio/Plug-Ins/VST3/Clamp.vst3 and ~/Library/Audio/Plug-Ins/Components/Clamp.component"
