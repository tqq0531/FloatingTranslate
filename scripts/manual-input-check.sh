#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/.."
STAGING="$(mktemp -d /tmp/fuyi-manual-check.XXXXXX)"
trap 'rm -rf "$STAGING"' EXIT
APP="$STAGING/ManualInputCheck.app"
mkdir -p "$APP/Contents/MacOS"
swiftc -swift-version 5 -framework AppKit -framework Carbon Tests/ManualInputCheck.swift -o "$APP/Contents/MacOS/ManualInputCheck"
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><dict><key>CFBundleIdentifier</key><string>com.fuyi.ManualInputCheck</string><key>CFBundleExecutable</key><string>ManualInputCheck</string><key>CFBundlePackageType</key><string>APPL</string><key>NSPrincipalClass</key><string>NSApplication</string></dict></plist>
PLIST
xattr -cr "$APP"
codesign --force --sign - "$APP"
open -W -n "$APP"
