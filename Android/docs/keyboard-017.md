# Android 0.1.7 — typing and dictionaries / 输入与词库

## What changed / 本版变化

- Incomplete single syllables (`d` → 的, `w` → 我, `zh` includes 中); phrase initials and mixed full/initial spelling (`nh`, `nih`, `nhao` → 你好).
- Built-in basic dictionary or basic + 30,000-word Rime Ice extension, selectable in Settings → 拼音词库. Both are offline; no keyboard text is uploaded. Changes retain the active composition and load on a background thread.
- Stable keyboard position when the candidate strip replaces the toolbar, wider space bar, tighter key gutters, larger letter labels, letter press previews and optional haptic feedback.
- System/light/dark appearance and height slider retained; restore-default-height action added in both keyboard panel and settings.

## References / 参考

Layout/interaction research: Fcitx5 Android `TextKeyboard.kt`, `ThemePrefs.kt` (https://github.com/fcitx5-android/fcitx5-android); Trime (https://github.com/osfans/trime). No upstream UI implementation or brand assets copied. This is not an exact reproduction of Sogou's settings or prediction engine.

Dictionary: https://github.com/iDvel/rime-ice, `cn_dicts/base.dict.yaml`, retrieved 2026-10-26. The bundled source is `app/src/main/assets/dictionary/ice-common.dict.yaml`. Its GPL-3.0 license, origin/changes/hash and upstream provenance notes are under `app/src/main/assets/licenses/RIME-ICE-*`. Selected the 30,000 highest-weight additional entries of 2–4 syllables not already in the basic dictionary; scaled weights by 0.05 to retain the basic single-character ranking. This is a subset, not the complete Rime Ice engine or lexicon.

Limitations: the app still uses its Java dictionary matcher, not native Rime/Fcitx. It does not provide Sogou's proprietary cloud suggestions, full contextual language-model ranking, or personal word learning. Offline dictionary coverage and typo handling remain less capable than those full engines.
