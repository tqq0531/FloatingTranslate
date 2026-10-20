#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/.."
mkdir -p build
source scripts/test-rime-support.sh
swiftc "${RIME_ARGS[@]}" Sources/SentenceBuffer.swift Sources/PinyinEngine.swift Sources/InputPrivacyPolicy.swift Sources/PanelPlacement.swift Sources/ShiftTapDetector.swift Tests/main.swift -o build/engine-tests
build/engine-tests Resources/pinyin_simp.dict.yaml
swiftc "${RIME_ARGS[@]}" -parse-as-library -target "$(uname -m)-apple-macos26.0" -framework Translation Tests/TranslationSmoke.swift -o build/translation-smoke
build/translation-smoke
swiftc "${RIME_ARGS[@]}" -parse-as-library -target "$(uname -m)-apple-macos26.0" \
  -framework AppKit -framework SwiftUI -framework Translation -framework Carbon \
  Sources/PinyinEngine.swift Sources/InputPrivacyPolicy.swift Sources/PrivacyProtection.swift \
  Sources/TranslationService.swift Sources/FloatingPanel.swift Sources/PanelPlacement.swift \
  Tests/PrivacySmoke.swift -o build/privacy-smoke
build/privacy-smoke
python3 scripts/audit-privacy.py
plutil -lint Info.plist
python3 scripts/check-input-bundle.py
build/浮译.app/Contents/MacOS/RegisterInputSource --self-test
STAGING="$(mktemp -d /tmp/fuyi-verify.XXXXXX)"
trap 'rm -rf "$STAGING"' EXIT
ditto --noextattr --norsrc build/浮译.app "$STAGING/浮译.app"
xattr -cr "$STAGING/浮译.app"
codesign --verify --deep --strict "$STAGING/浮译.app"
