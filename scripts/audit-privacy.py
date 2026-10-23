#!/usr/bin/env python3
"""Small source audit, not a proof about macOS or every third-party app."""
from pathlib import Path
import re
root = Path(__file__).resolve().parent.parent
sources = {p.name: p.read_text() for p in (root / "Sources").glob("*.swift")}
combined = "\n".join(sources.values())
schema = (root / "Resources/RimeData/fuyi.schema.yaml").read_text()
bridge = (root / "Bridge/FuyiRime.c").read_text()
checks = {
    "Rime user dictionary and encoder disabled": "enable_user_dict: false" in schema and "enable_encoder: false" in schema,
    "Rime learning disabled and ordinary logs suppressed": '"_no_learning", 1' in bridge and "min_log_level = 3" in bridge,
    "Rime bridge has no network client": not re.search(r"\b(connect|socket|sendto|curl_easy_perform)\s*\(", bridge),
    "no app-level HTTP, sockets, or WebKit clients": not re.search(r"URLSession|URLRequest|NWConnection|NWListener|WKWebView|import Network|import Darwin", combined),
    "no keyboard event tap or accessibility scraping": not re.search(r"CGEvent\.tapCreate|CGEventTapCreate|addGlobalMonitorForEvents|AXUIElement|IOHIDManager", combined),
    "no clipboard reads": not re.search(r"NSPasteboard\s*\.\s*general\s*\.\s*(?:string|readObjects|data|pasteboardItems)", combined),
    "no disk-backed translation cache": not re.search(r"\bcache\s*[:=\[]|\.write\s*\(", sources["TranslationService.swift"]),
    "secure input is checked at translation boundary": sources["TranslationService.swift"].count("PrivacyProtection.requireAllowed()") == 2,
    "only installed-language session used for translation": "TranslationSession(installedSource:" in sources["TranslationService.swift"],
    "persistent settings are an explicit small allowlist": set(re.findall(r'forKey: "([^"]+)"', combined)) <= {"englishDefault", "translationAbove", "privacyMode", "didShowWelcome", "directEnglish", "chineseDefault", "rememberCandidates"},
    "no mouse-position fallback for text placement": "NSEvent.mouseLocation" not in combined,
}
for name, passed in checks.items():
    print(("PASS " if passed else "FAIL ") + name)
if not all(checks.values()):
    raise SystemExit(1)
