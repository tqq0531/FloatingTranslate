# Android Rime core / 安卓原生拼音核心

0.2.0 replaces primary candidate decoding with native Rime; the Java dictionary remains a fallback and supplies optional extension candidates. The keyboard UI, encrypted choice memory and offline translation remain FloatingTranslate code. The native Rime user dictionary is disabled: Java's encrypted local word memory continues to rank choices. This is not a claim of commercial IME prediction quality.

0.2.0 由 Rime 原生引擎优先生成候选；Java 词库保留为兼容回退和扩展候选来源。原生用户词典关闭，个人选词继续由浮译的本地加密记忆处理。不宣称已达到商业输入法预测水平。

## Pinned binary and corresponding source

`librime_jni.so` is extracted without modification from the official Trime v3.3.12 release (e09ac711). Its exported Rime C API reports 1.17.0. We load the C API through dlopen; Trime's JNI frontend and UI are not invoked. The combined Android distribution includes GPL-3.0-or-later components; its source and build instructions are provided publicly alongside the application. Preserve Trime's GPL notice and all dependency licenses when redistributing.

- Official binaries: https://github.com/osfans/trime/releases/tag/v3.3.12
- Corresponding upstream source/build instructions: https://github.com/osfans/trime/tree/v3.3.12 (clone recursively to include pinned native dependencies).
- FloatingTranslate bridge and application source: this repository's `codex/android-ime` branch, `Android/app/src/main/cpp` and `Android/app/src/main/java`.
- Rime C header: librime 1.11.2 stable ABI, BSD license. Native library preserves compatible API structure; optional functions are accessed via their C API slots.
- Full GPL text bundled at `assets/licenses/Trime-GPL-3.0.txt`; Rime BSD notice at `assets/licenses/librime-LICENSE.txt`.

## Rebuild

1. `python3 tools/prepare_rime.py` fetches the exact upstream release assets, verifies pinned SHA256, and extracts each ABI's native library.
2. Set `ANDROID_NDK_HOME` to Android NDK r27c (27.2.12479018), then run `tools/build_rime_bridge.sh`. The bridge is built for all four ABIs with 16 KB ELF alignment. That does not guarantee 16 KB alignment of every upstream library or translation dependency.
3. Rime schema/dictionary binaries are committed in `assets/rime/build`; their source is `Resources/RimeData/fuyi.schema.yaml` and `Resources/pinyin_simp.dict.yaml`. Regenerate with the Rime deployer on a build machine. These are portable data files, not host machine code.
4. Build the APK normally. The phone performs no native-library, dictionary or translation download.

Use `NativePinyinDeviceTest` to assert native startup (rather than silently accepting fallback), version, abbreviated/full input and whole-sentence typo handling.
