from pathlib import Path
import plistlib
root = Path(__file__).resolve().parent.parent
app = root / 'build/浮译.app'
info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
assert info['InputMethodConnectionName'] == info['CFBundleIdentifier'] + '_Connection'
assert info['LSBackgroundOnly'] is True
mode = info['CFBundleIdentifier'] + '.Hans'
assert info['ComponentInputModeDict']['tsVisibleInputModeOrderedArrayKey'] == [mode]
setup = app / 'Contents/Helpers/FuyiInputSetup.app'
child = plistlib.loads((setup / 'Contents/Info.plist').read_bytes())
assert child['CFBundleIdentifier'] != info['CFBundleIdentifier']
assert child['CFBundleVersion'] == info['CFBundleVersion']
assert (setup / 'Contents/MacOS/FuyiInputSetup').is_file()
assert (setup / 'Contents/Resources/FuyiApp.icns').is_file()
assert 'FYDIAG' not in (root / 'Sources/InputController.swift').read_text()
print('PASS modern IMK connection, background identity, one visible mode, separate activation client, matching versions, no temporary diagnostics')
