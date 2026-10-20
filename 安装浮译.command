#!/bin/zsh
set -euo pipefail
cd "${0:A:h}"
SOURCE="$PWD/浮译.app"
if [[ ! -d "$SOURCE" ]]; then SOURCE="$PWD/build/浮译.app"; fi
if [[ ! -x "$SOURCE/Contents/MacOS/FloatingTranslate" ]]; then
  print '找不到应用。请完整解压安装包，或先运行 scripts/build.sh。'
  exit 1
fi
# Uses the same checked, staged installation as the graphical installer.
# Never removes quarantine, disables Gatekeeper, or downloads code.
"$SOURCE/Contents/MacOS/FloatingTranslate" --install
open "$HOME/Library/Input Methods/浮译.app" --args --settings
