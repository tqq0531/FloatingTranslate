#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/.."
source scripts/test-rime-support.sh
STAGING="$(mktemp -d /tmp/fuyi-controller-check.XXXXXX)"
trap 'rm -rf "$STAGING"' EXIT
APP="$STAGING/ControllerFixture.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
ditto --noextattr --norsrc Resources "$APP/Contents/Resources"
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><dict><key>CFBundleIdentifier</key><string>com.fuyi.ControllerFixture</string><key>CFBundleExecutable</key><string>ControllerFixture</string><key>CFBundlePackageType</key><string>APPL</string><key>NSPrincipalClass</key><string>NSApplication</string></dict></plist>
PLIST
swiftc "${RIME_ARGS[@]}" -parse-as-library -swift-version 5 -target "$(uname -m)-apple-macos26.0" \
 -framework AppKit -framework InputMethodKit -framework SwiftUI -framework Translation -framework Carbon \
 Sources/PinyinEngine.swift Sources/SentenceBuffer.swift Sources/ShiftTapDetector.swift Sources/InputController.swift Sources/InputSession.swift \
 Sources/InputPrivacyPolicy.swift Sources/PrivacyProtection.swift Sources/TranslationService.swift Sources/FloatingPanel.swift Sources/PanelPlacement.swift \
 Tests/ControllerSmoke.swift -o "$APP/Contents/MacOS/ControllerFixture"
"$APP/Contents/MacOS/ControllerFixture"

mv "$APP/Contents/Resources/RimeData" "$STAGING/RimeData-disabled"
"$APP/Contents/MacOS/ControllerFixture" --missing-engine
