# 浮译 / FloatingTranslate

输入中文拼音时，实时显示当前整句的英文翻译，可选择默认输出中文或英文。支持分段输入后累积整句、中文 / English 切换和英文选中复制。

A Pinyin input method that previews an English translation of the current sentence as you type. Choose Chinese or English output, build sentences across multiple committed phrases, switch input languages, and select or copy translations.

## 下载与平台分支 / Downloads and platform branches

请下载 Release 中的安装包；GitHub 自动生成的 Source code 压缩包不包含完整模型和运行库。

Download the packaged assets from Releases. GitHub's automatically generated source archives do not contain the complete models or runtime libraries.

| 平台 / Platform | 版本与安装包 / Version and package | 分支 / Branch | 要求与状态 / Requirements and status |
| --- | --- | --- | --- |
| macOS | [0.5.11 ZIP](https://github.com/tqq0531/FloatingTranslate/releases/tag/v0.5.11) | [codex/macos-ime](https://github.com/tqq0531/FloatingTranslate/tree/codex/macos-ime) | macOS 26+，Apple Silicon 已验证，试用版。 / Tested on Apple Silicon; trial build. |
| Android 安卓 | [0.1.4 离线 APK / Offline APK](https://github.com/tqq0531/FloatingTranslate/releases/tag/android-v0.1.4) | [codex/android-ime](https://github.com/tqq0531/FloatingTranslate/tree/codex/android-ime) | Android 8+，调试签名试用版；16 KB 内存页设备未保证兼容。 / Debug-signed trial; 16 KB page-size devices are not guaranteed. |
| Windows | [0.1.1 x64 离线 ZIP / Offline ZIP](https://github.com/tqq0531/FloatingTranslate/releases/tag/windows-v0.1.1) | [codex/windows-ime](https://github.com/tqq0531/FloatingTranslate/tree/codex/windows-ime) | Windows 10/11 x64 开发测试版，尚未在真实 Windows 宿主完成验收。 / Development test build; actual Windows host validation remains pending. |

默认分支提供三平台总览，平台分支分别保留对应版本的实现。部分目录是共享词库、模型准备脚本或历史代码，目录存在不代表该分支发布其他平台。

The default branch provides the cross-platform overview. Platform branches identify the corresponding version of each implementation. Shared dictionaries, model preparation scripts, and historical directories may remain in a platform branch.

## 安装 / Installation

- **Mac：**完整解压，打开“浮译.app”→“安装 / 修复输入法”→“启用并试用”。首次准备 Apple 中英语言包需要联网，之后使用本机模型。本包未经过 Apple Developer ID 签名公证。
- **Mac:** Extract the ZIP, open 浮译.app, choose Install / Repair, then Enable and Try. Apple language models require an initial download; translation then runs locally. The trial package is not Developer ID signed or notarized.
- **安卓：**安装完整 APK，在系统设置启用浮译并选择为当前键盘。首次打开会从 APK 解压并校验内置模型，显示实际进度；手机不需要下载语言包。
- **Android:** Install the complete APK, enable FloatingTranslate in system keyboard settings, and select it. First launch extracts and verifies the bundled models with progress. No language-model download is needed on the phone.
- **Windows：**完整解压到固定本机目录，双击“启用浮译.cmd”，批准首次管理员注册，再按 Win + 空格切换。模型、词库、推理库与必要的微软运行库安装程序已随包提供，不需另外下载。启用期间不要移动或删除目录。
- **Windows:** Extract to a permanent local folder, run 启用浮译.cmd, approve the initial administrator registration, then select it with Win + Space. Models, dictionaries, inference libraries, and the required Microsoft runtime installer are bundled. Keep the folder in place while enabled.

Windows 原生 TSF 输入法首次必须向系统注册；“解压使用”不代表零注册。此版只包含 x64 组件，32 位应用、ARM64 原生应用、UWP 和管理员应用尚未支持或验收。

A native Windows TSF input method requires initial system registration. This extractable package does not eliminate registration. Only x64 components are included; 32-bit apps, native ARM64 apps, UWP, and elevated apps are not supported or validated in this release.

## 功能与差异 / Features and differences

| 功能 / Feature | macOS | Android | Windows |
| --- | --- | --- | --- |
| 整句英文预览 / Sentence translation preview | Apple 本机翻译 / Apple local translation | 内置 OPUS-MT / Bundled OPUS-MT | 内置 OPUS-MT / Bundled OPUS-MT |
| 拼音、简拼与纠错 / Pinyin, abbreviations and typo handling | Rime | Java 本地词典，能力较少 / Local Java dictionary, more limited | Rime |
| 中英文切换、默认输出语言 / Input switching and default output language | ✓ | ✓ | ✓ |
| 英文复制 / Copy English | 拖选或按钮 / Selection or button | 长按译文 / Long press | 拖选或按钮 / Selection or button |
| 24 小时剪贴板历史 / 24-hour clipboard history | 无 / No | 可选、本地加密 / Optional, locally encrypted | 无 / No |

各平台的引擎和翻译质量不同，不承诺译文、候选排序或宿主兼容性完全相同。Windows 已完成交叉编译、核心逻辑和相同模型推理验证，但这些不能代替真实 Windows 输入测试。

Engines and translation quality differ across platforms. Translations, candidate ranking, and application compatibility may vary. Windows cross-compilation, core logic, and shared model inference checks have passed; these do not replace actual Windows input testing.

## 隐私与密码 / Privacy and passwords

- 安卓 0.1.4 无 `INTERNET` 权限，模型随 APK 提供。Windows 随包模型在本机 CPU 推理，应用没有云翻译后备接口、遥测或自动更新。Mac 使用 Apple 本机翻译，首次语言包准备需要联网；系统自身的网络行为不受本项目控制。
- Android 0.1.4 has no `INTERNET` permission. Windows performs bundled-model inference locally, with no cloud translation fallback, telemetry, or automatic updates. Mac uses Apple local translation and requires an initial language-model download. Operating-system networking is outside this project's control.
- 不保存连续输入正文或译文历史。Mac 0.5.13 新增可关闭/清除的本机钥匙串短词选词计数，其中可能含私人短语。安卓的剪贴板历史为单独的可选功能，使用 Android Keystore / AES-GCM 本地加密，从复制时间起保留 24 小时；重复复制不延长期限。系统休眠或强行停止可能延迟物理删除，但到期内容不再显示或允许插入。
- Continuous prose and translation history is not saved. Mac 0.5.13 adds optional, clearable short-candidate counts in the local Keychain; these may contain private phrases. Android clipboard history is a separate optional feature, encrypted locally with Android Keystore / AES-GCM and retained for 24 hours from copying. Duplicates do not extend expiry. Sleep or force-stop can delay physical deletion, but expired entries cannot be shown or inserted.
- 检测到受保护输入框或启用手动隐私模式时停止处理，但无法保证识别所有自定义密码框。**输入密码、验证码和密钥时，请切换系统自带键盘；不要将这些内容保存到剪贴板历史。**
- Protected fields and manual privacy mode disable processing, but custom password fields may not be identified reliably. **Use the system keyboard for passwords, verification codes, and secrets; do not retain them in clipboard history.**

## 文档、开发与许可 / Documentation, development and licenses

| 平台 / Platform | 使用与构建 / Usage and build | 验证记录 / Validation |
| --- | --- | --- |
| Mac | [Mac 使用文档 / Mac guide](https://github.com/tqq0531/FloatingTranslate/blob/codex/macos-ime/README.macOS.md) · [隐私 / Privacy](https://github.com/tqq0531/FloatingTranslate/blob/codex/macos-ime/docs/PRIVACY.md) | [兼容性记录 / Compatibility](https://github.com/tqq0531/FloatingTranslate/blob/codex/macos-ime/docs/COMPATIBILITY.md) |
| Android | [安卓文档 / Android guide](https://github.com/tqq0531/FloatingTranslate/blob/codex/android-ime/Android/README.md) | [测试记录 / Tests](https://github.com/tqq0531/FloatingTranslate/blob/codex/android-ime/Android/TESTING.md) |
| Windows | [Windows 文档 / Windows guide](https://github.com/tqq0531/FloatingTranslate/blob/codex/windows-ime/Windows/README.md) | [测试范围 / Test boundaries](https://github.com/tqq0531/FloatingTranslate/blob/codex/windows-ime/Windows/TESTING.md) |

开发电脑首次构建需要下载依赖；安卓与 Windows 大型模型不提交进 Git，请按平台构建文档准备固定版本并校验。面向用户的完整安装包已包含对应离线资源，Mac 的 Apple 模型由系统管理。每个 Release 提供 SHA-256 校验文件。

Initial development builds require dependency downloads. Large Android and Windows models are excluded from Git; follow the platform instructions to fetch and verify pinned artifacts. User packages include their required offline resources, except Apple's system-managed Mac models. Each release provides SHA-256 checksums.

第三方组件保持各自许可：Rime BSD、拼音词库 Apache 2.0、OPUS-MT 模型 CC BY 4.0、ONNX Runtime / Extensions MIT；其余声明见各平台文档和安装包。Microsoft 运行库受其独立条款约束。

Third-party components retain their own licenses: Rime BSD, Pinyin dictionaries Apache 2.0, OPUS-MT CC BY 4.0, and ONNX Runtime / Extensions MIT. See platform documentation and packaged notices for details. Microsoft runtime components have separate terms.

## Mac 0.5.13 开发版 / Mac development build

- 轻按 Shift 切换拼音和 English 英译中。英文空格继续当前句子；回车提交默认语言、Tab 另一语言、Esc 保留英文原文。
- 设置中新增独立的“英文转换后默认输出中文”，不影响“拼音转换后默认输出英文”。浮窗中文译文标记“中”，支持按钮复制和拖选复制。
- 拼音候选按 `=` 下一页、`-` 上一页，每页五项，最多 100 项；主动常选的短词会优先。选词计数保存在本机钥匙串，可关闭或清除，不保存连续正文。
- 保留“因为这是mac版本的”等混输整句翻译。Mac 使用 Apple 本机模型；首次语言包仍由系统准备，未把 Windows 模型替换到 Mac。

Tap Shift for Pinyin or English-to-Chinese. English word spaces stay within the sentence; Enter commits the default, Tab the alternative and Esc the source. Independent output defaults are available for each direction. Use `=` / `-` for five-item candidate pages, up to 100 entries. Optional, clearable short-candidate counts use the local Keychain; prose and translations are not saved. Mixed-language sentence context remains intact. Apple language packs still need initial system preparation.

See [usage and checks](docs/mac-0.5.13.md) and [privacy](docs/PRIVACY.md).

Mac 0.5.14：中文模式输入 `make` 后轻按 Shift，提交原始英文 `make`，不接受当前“马克”等拼音候选；已确认的中文前缀和整句翻译上下文继续保留。此行为不受两个默认输出开关影响。

Mac 0.5.14: tapping Shift after typing `make` in Pinyin commits the literal `make`, rather than the Chinese candidate preview. Confirmed prefixes and mixed-sentence translation context are retained, independent of the output defaults.

Mac 0.5.15：英文输入使用透明背景和无下划线的组合文字，光标停在句末；保留实时整句翻译和回车 / Tab 输出选择。中文拼音仍保留常规组词提示。第三方编辑器可能自行覆盖组合文字样式。

Mac 0.5.15: English composition uses a transparent background with no underline and leaves the caret at the end. Live sentence translation and Enter / Tab output selection remain available. Pinyin retains its standard composition appearance. Third-party editors may override composition styling.

Mac 0.5.16：尚未上屏的分段拼音中，退格先撤回最近一次选词并恢复拼音，保留未选部分；整句上屏后仍按普通退格删除。
Backspace reopens the last selected segment in an active Pinyin composition, preserving its remaining input. Committed text uses normal deletion.

Mac 0.5.17：候选框采用更紧凑的系统风格，候选字号 15 pt、译文字号 13 pt，常规双行高度由 78 pt 缩至约 57 pt；宽度随内容调整，保留透明圆角、翻译复制和退格重新选词。

Mac 0.5.17: the compact panel uses 15 pt candidates and 13 pt translations, reducing its usual two-row height from 78 pt to about 57 pt. Width adapts to content while preserving transparent corners, translation copying, and Backspace reselection.
