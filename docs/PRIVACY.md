# 隐私、联网与密码输入

适用版本：浮译 0.5.13 本地试用版。以下说明对应此仓库源码，不是对其他来源安装包、macOS 或所有目标应用的安全承诺。

## 输入内容会发到哪里？

浮译把本次输入的当前句子（包括已逐词确认的中文和正在编辑的完整中文候选）交给 macOS 的 Apple Translation 框架，在设备上生成英文或中文。

- 没有开发者服务器、第三方翻译 API、登录账户、广告或统计 SDK。
- 应用源码没有 HTTP 客户端、socket、网页组件或云翻译降级路径。
- 常规翻译使用 `TranslationSession(installedSource:target:)`，只使用已安装的语言包。缺少语言包时提示用户准备，不会偷偷切换到在线翻译。
- 首次下载、重新准备或由系统更新中英语言包，需要连接 Apple 服务。这与翻译正文发送到开发者服务器是不同的行为。
- Apple 的文档说明，TranslationSession 翻译在设备上执行；Apple **可能收集应用标识、源/目标语言等使用与性能指标，但不包含原文或译文正文**。是否以及何时发生此类系统通信，由 macOS 管理，浮译不能保证“整台电脑绝不联网”。

因此，准确的描述是：**正文在本机翻译，浮译不主动上传输入内容；准备语言包需要联网，不应宣传为完全不联网。**

语言包安装完成后，可在关闭网络的情况下自行验证翻译。不要用真实密码测试。

## 本地保存了什么？

| 数据 | 保存位置 / 生命周期 |
| --- | --- |
| 内置拼音词库 | 应用 Resources，随应用分发 |
| 中英文输入模式、两个方向的默认输出语言、短词记忆开关、译文条位置、隐私模式、是否展示欢迎窗口 | macOS UserDefaults，布尔设置 |
| 当前拼音、候选词、当前句子和译文 | 仅内存；逐词上屏后继续累计当前句子；取消、光标移动、切换应用 / 输入法、安全输入等操作清除。未完句停顿 60 秒、句末 8 秒后清除 |
| Rime 运行配置 | 系统临时目录 `fuyi-rime-进程号/`，可含方案选择 / 版本等 `user.yaml` 元数据；不含输入历史。程序异常退出可能留下临时元数据 |
| 设置窗口中的试用文本 | 仅当前窗口的内存状态；关闭窗口或开启隐私模式时清除 |
| 连续正文、译文历史、Rime 用户词库 | 不建立、不写入磁盘 |
| 可选短词选词计数 | macOS 登录钥匙串，服务 `com.fuyi.inputmethod.CandidateCounts`；仅主动选定的最多四个汉字候选、拼音和次数，上限 2048 项；可能含人名或私人短语。可关闭应用/记录，或清除已保存项。关闭开关不会删除旧项；钥匙串锁定时清除会提示重试。钥匙串不可用时仅内存处理，不弹出认证提示，不以明文文件降级 |
| Apple 语言包 | macOS 自行管理 |
| 升级备份 | `~/Library/Application Support/FloatingTranslate/Backups/`，仅旧应用文件，不备份输入文本 |

本版使用随应用打包的 Rime，关闭用户词典、用户造词和学习选项，不生成用户词频数据库。普通 Rime 日志关闭；系统异常诊断仍受 macOS 管理。分段上下文只记录通过浮译输入的当前句子，不扫描目标文档，也不读取光标之前的段落。

浮译不会主动记录输入正文日志、读取剪贴板内容、扫描其他应用文档或安装全局按键监听。用户主动在设置中点击“复制译文”，或在浮窗拖选英文后复制、点击整句复制按钮时，会把相应译文**写入**系统剪贴板；之后的剪贴板历史由系统或其他剪贴板软件管理。

鼠标选择英文时会暂时冻结显示。若此时系统结束输入会话，只在浮窗内存中暂存当前可见英文，供复制使用，最多 15 秒；浮窗失去键盘焦点、隐私模式或安全输入生效时清理。不会建立磁盘上的译文历史。

“清除引用”不等于对内存做密码学擦除。Swift 字符串、Apple 框架的内部内存、macOS 交换空间和系统诊断不受本项目完全控制。

## 密码框会怎样？请重点阅读

1. 使用正确的系统密码控件或启用 Secure Input 的应用，通常会由 macOS 对输入方式施加限制。具体行为取决于目标应用。
2. 浮译额外检查 `IsSecureEventInputEnabled()`：安全输入开启时，停止处理按键，不进行中文转换或翻译；取消待处理请求，清空候选栏和英文浮窗。活动组合期间也会定时检查，避免只在按键到来时检测。
3. 翻译开始前和返回结果后均检查安全输入 / 隐私模式，丢弃不该展示的结果。
4. 设置和菜单中提供**隐私模式**，直接输入英文字符，暂停拼音转换和翻译。

**输入密码、验证码、恢复码、密钥时，建议直接切换到 macOS 自带的 ABC 输入法；不要把密码输入浮译的试用框。**

不能写成“密码绝对不会泄漏”：有些网站或应用使用没有开启安全输入的自定义密码框，浮译无法可靠判断某串普通文本是不是密码。把密码打到普通文本框、聊天框或翻译试用框，也不会自动获得密码框保护。本机处理减少了向翻译服务传输正文的风险，但不能替代目标应用本身的安全实现。

## 权限与安装程序

浮译通过 InputMethodKit 与当前选中它的文本应用通信，不要求辅助功能、屏幕录制或全局输入监控权限。本版本安装到当前用户目录，不要求管理员密码。

安装程序不移除下载文件的 quarantine 标记、不关闭 Gatekeeper，也不在安装时下载脚本或依赖。当前本地构建采用临时签名，尚未取得 Developer ID 公证；打开下载版时可能需要到“系统设置 → 隐私与安全性”按 macOS 提示选择“仍要打开”。只对你信任且校验过的版本这样做。

## 如何核查

- 运行 `python3 scripts/audit-privacy.py` 做源代码检查。这只能检查所列项目，不能证明整个系统完全安全。
- `./scripts/test.sh` 包含真实 Secure Input 拦截测试、隐私模式拦截测试以及隐藏浮窗后清除文本的检查；全部使用固定测试句，不使用真实密码。
- 查看 `Sources/TranslationService.swift`、`Sources/PrivacyProtection.swift`、`Sources/InputController.swift`、`Sources/InputSession.swift` 和 `Sources/FloatingPanel.swift`。
- 向开发者反馈问题时，使用虚构句子复现；不要提交密码、聊天记录、密钥或包含这些内容的截图。

参考：[Apple TranslationSession](https://developer.apple.com/documentation/translation/translationsession)、[Apple Secure Event Input 技术说明](https://developer.apple.com/library/archive/technotes/tn2150/_index.html)。

安装菜单修复会备份系统的已启用输入源清单（只有输入法标识与配置，不含输入文字），保存在 `~/Library/Application Support/FloatingTranslate/InstallBackups/`。仅补齐浮译自身缺失的两项登记。

“启用并试用”是随应用附带的普通文本窗口，通过它自己的系统输入上下文选择浮译。它不监听全局键盘、不使用辅助功能读取其他软件，不联网，也不保存试用文字；关闭窗口会清空文字。安装检查优先使用新版 macOS 的 com.apple.inputsources / AppleEnabledThirdPartyInputSources，并保留旧版清单兼容检查。

## 0.5.13 双向翻译与短词记忆 / Bidirectional translation and selection counts

英文模式按整句进行英译中，空格不拆成独立请求。“英文转换后默认输出中文”与拼音默认输出英文设置独立。英文连续正文不会写入短词记录；数字、拉丁字母、长句和自动候选预览均不记录。Rime 自身学习仍关闭。

Candidate counts are optional, local Keychain data containing only explicitly selected short Chinese phrases, their original Pinyin, and bounded counts. They may contain names or private phrases. Disabling stops applying/writing counts; clearing deletes the saved item when Keychain access succeeds. There is no plaintext fallback, automatic sync, or translation/prose history. This is input-related local data: do not describe this version as saving no input-related information at all.

The two directions use Apple's installed-language translation sessions. Text stays on-device; initial language preparation contacts Apple. See [Apple TranslationSession documentation](https://developer.apple.com/documentation/Translation/TranslationSession).
