#!/bin/sh
# Builds kinoz.icns from the 256 px PNG using only tools that ship with macOS (sips, iconutil).
#   make-icns.sh <source.png> <output.icns>
set -e
src="$1"; out="$2"
tmp="$(mktemp -d)/kinoz.iconset"
mkdir -p "$tmp"
for s in 16 32 64 128 256; do
  sips -z "$s" "$s" "$src" --out "$tmp/icon_${s}x${s}.png" >/dev/null
done
# the @2x names reuse the next size up (a 256 px source cannot honestly fill 512 or 1024)
cp "$tmp/icon_32x32.png"   "$tmp/icon_16x16@2x.png"
cp "$tmp/icon_64x64.png"   "$tmp/icon_32x32@2x.png"
cp "$tmp/icon_256x256.png" "$tmp/icon_128x128@2x.png"
cp "$tmp/icon_256x256.png" "$tmp/icon_256x256@2x.png"
rm "$tmp/icon_64x64.png"
iconutil -c icns "$tmp" -o "$out"
