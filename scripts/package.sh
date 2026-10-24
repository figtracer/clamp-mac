#!/usr/bin/env bash
# Builds universal release binaries and packages them as a .pkg installer and
# clean zips in Clamp/build-universal/dist.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
version="$(sed -n 's/^#define PLUG_VERSION_STR "\(.*\)"/\1/p' "$root/Clamp/config.h")"
build="$root/Clamp/build-universal"
dist="$build/dist"
pkg="$build/pkg"
plugins="Library/Audio/Plug-Ins"

cmake -S "$root/Clamp" -B "$build" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$build" -j"$(sysctl -n hw.ncpu)"

for bundle in Clamp.vst3 Clamp.component; do
  codesign --force --deep -s - "$build/out/$bundle"
  codesign --verify --deep --strict "$build/out/$bundle"
done

rm -rf "$dist" "$pkg"
mkdir -p "$dist" "$pkg/root/$plugins/VST3" "$pkg/root/$plugins/Components"

# Zips without AppleDouble (._*) metadata files.
ditto -c -k --norsrc --noextattr --keepParent "$build/out/Clamp.vst3" "$dist/Clamp-Port-$version-macOS-VST3.zip"
ditto -c -k --norsrc --noextattr --keepParent "$build/out/Clamp.component" "$dist/Clamp-Port-$version-macOS-AU.zip"

# Installer for /Library/Audio/Plug-Ins. Files installed by Installer get no
# quarantine flag, so no xattr step is needed.
ditto --norsrc --noextattr "$build/out/Clamp.vst3" "$pkg/root/$plugins/VST3/Clamp.vst3"
ditto --norsrc --noextattr "$build/out/Clamp.component" "$pkg/root/$plugins/Components/Clamp.component"
pkgbuild --analyze --root "$pkg/root" "$pkg/components.plist"
# Never let Installer "relocate" the bundles to another copy with the same ID.
for i in 0 1; do
  /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$pkg/components.plist" 2>/dev/null ||
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$pkg/components.plist"
done
COPYFILE_DISABLE=1 pkgbuild --root "$pkg/root" --component-plist "$pkg/components.plist" \
  --identifier com.clamp-mac.clampport --version "$version" --install-location / "$pkg/ClampPort.pkg"

cat > "$pkg/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Clamp Port $version</title>
  <welcome mime-type="text/plain"><![CDATA[Installs Clamp Port, an unofficial macOS port of Outobugi's Clamp clipper.

VST3: /$plugins/VST3/Clamp.vst3
AU:   /$plugins/Components/Clamp.component

Rescan plugins in your DAW afterwards. It is listed as "Clamp Port" by "clamp-mac". Not affiliated with Outobugi.]]></welcome>
  <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
  <volume-check><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
  <choices-outline><line choice="default"/></choices-outline>
  <choice id="default" title="Clamp Port"><pkg-ref id="com.clamp-mac.clampport"/></choice>
  <pkg-ref id="com.clamp-mac.clampport" version="$version">ClampPort.pkg</pkg-ref>
</installer-gui-script>
EOF
COPYFILE_DISABLE=1 productbuild --distribution "$pkg/distribution.xml" --package-path "$pkg" \
  "$dist/Clamp-Port-$version-macOS-Installer.pkg"

(cd "$dist" && shasum -a 256 ./*.zip ./*.pkg | sed 's# \./# #' > SHA256SUMS.txt)
ls -la "$dist"
