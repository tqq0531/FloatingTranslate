#!/bin/zsh
set -euo pipefail
cd "${0:A:h}/.."
mkdir -p build
source scripts/test-rime-support.sh
swiftc "${RIME_ARGS[@]}" -parse-as-library -target "$(uname -m)-apple-macos26.0" \
  -framework AppKit -framework SwiftUI -framework Translation -framework Carbon \
  Sources/SentenceBuffer.swift Sources/PinyinEngine.swift Sources/InputPrivacyPolicy.swift Sources/PrivacyProtection.swift \
  Sources/TranslationService.swift Sources/FloatingPanel.swift Sources/PanelPlacement.swift \
  Tests/PanelSmoke.swift -o build/panel-smoke
build/panel-smoke Resources/pinyin_simp.dict.yaml "$PWD/build/panel-preview.png"
