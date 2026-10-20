#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/.."
ROOT="$PWD"
STAGING="$(mktemp -d /tmp/fuyi-build.XXXXXX)"
trap 'rm -rf "$STAGING"' EXIT
APP="$STAGING/浮译.app"
OUTPUT_APP="$ROOT/build/浮译.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$ROOT/build"
ditto --noextattr --norsrc Resources "$APP/Contents/Resources"
swift scripts/make-icon.swift "$APP/Contents/Resources/FuyiKeycap-v3.pdf"
scripts/make-app-icon.sh "$APP/Contents/Resources/FuyiApp.icns"
python3 scripts/bundle-rime.py "$APP"
RIME_PREFIX="$(brew --prefix librime)"
clang -O2 -I"$RIME_PREFIX/include" -IBridge -c Bridge/FuyiRime.c -o "$STAGING/FuyiRime.o"
swiftc -swift-version 5 -O -target "$(uname -m)-apple-macos26.0" \
  -framework AppKit -framework SwiftUI -framework InputMethodKit -framework Translation \
  -import-objc-header Bridge/FuyiRime.h -Xlinker -rpath -Xlinker @executable_path/../Frameworks \
  Sources/*.swift "$STAGING/FuyiRime.o" "$APP/Contents/Frameworks/librime.1.dylib" -o "$APP/Contents/MacOS/FloatingTranslate"
swiftc -O -framework Carbon scripts/register-input.swift -o "$APP/Contents/MacOS/RegisterInputSource"
codesign --force --sign - "$APP/Contents/MacOS/RegisterInputSource"
SETUP_APP="$APP/Contents/Helpers/FuyiInputSetup.app"
mkdir -p "$SETUP_APP/Contents/MacOS" "$SETUP_APP/Contents/Resources"
swiftc -swift-version 5 -O -framework AppKit Setup/InputSetup.swift -o "$SETUP_APP/Contents/MacOS/FuyiInputSetup"
cp "$APP/Contents/Resources/FuyiApp.icns" "$SETUP_APP/Contents/Resources/FuyiApp.icns"
python3 - "$SETUP_APP/Contents/Info.plist" <<'PYPLIST'
import sys, plistlib
from pathlib import Path
parent=plistlib.loads(Path('Info.plist').read_bytes())
info={"CFBundleIdentifier": "com.fuyi.inputmethod.FloatingTranslate.InputSetup", "CFBundleExecutable": "FuyiInputSetup", "CFBundleName": "浮译启用", "CFBundleDisplayName": "浮译启用", "CFBundlePackageType": "APPL", "CFBundleIconFile": "FuyiApp", "NSPrincipalClass": "NSApplication", "LSMinimumSystemVersion": "26.0", "CFBundleShortVersionString": parent['CFBundleShortVersionString'], "CFBundleVersion": parent['CFBundleVersion']}
Path(sys.argv[1]).write_bytes(plistlib.dumps(info))
PYPLIST
codesign --force --sign - "$SETUP_APP"
cp Info.plist "$APP/Contents/Info.plist"
xattr -cr "$APP"
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
# Stage signing outside cloud-synced Documents to avoid Finder metadata races.
if [[ -d "$OUTPUT_APP" ]]; then rm -rf "$OUTPUT_APP"; fi
ditto --noextattr --norsrc "$APP" "$OUTPUT_APP"
print "已生成：$OUTPUT_APP"
