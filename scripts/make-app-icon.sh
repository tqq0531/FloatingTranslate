#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/.."
SOURCE="$PWD/Design/AppIcon/fuyi-app-icon-v2.png"
OUTPUT="${1:-$PWD/Resources/FuyiApp.icns}"
STAGING="$(mktemp -d /tmp/fuyi-app-icon.XXXXXX)"
trap 'rm -rf "$STAGING"' EXIT
ICONSET="$STAGING/FuyiApp.iconset"
mkdir -p "$ICONSET" "${OUTPUT:h}"
for SIZE in 16 32 128 256 512; do
    sips -z "$SIZE" "$SIZE" "$SOURCE" --out "$ICONSET/icon_${SIZE}x${SIZE}.png" >/dev/null
    DOUBLE=$((SIZE * 2))
    sips -z "$DOUBLE" "$DOUBLE" "$SOURCE" --out "$ICONSET/icon_${SIZE}x${SIZE}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$OUTPUT"
print "已生成 App 图标：$OUTPUT"
