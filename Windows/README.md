# 浮译 Windows · 0.1.5 开发试用版

Windows 原生 TSF 输入法，中文拼音使用与 Mac 相同的 Rime 1.17.0 词库和方案，中英双向翻译使用完整内置的 OPUS-MT / ONNX 本地模型。


## 0.1.5 英译中 / Offline English-to-Chinese

- 轻按 Shift 切换中文拼音和英文翻译模式。英文模式输入 `Have you worked out today?`，浮窗显示中文；空格用于分词，保留整句上下文。
- “拼音转换后默认输出英文”和新增“英文转换后默认输出中文”独立保存，默认均关闭。英文模式按回车提交默认语言，Tab 提交另一种语言；译文未就绪时保留组合等待。Esc 保留英文原文。
- 中文模式保持原选词流程；复制按钮和拖选复制同时支持两种方向的译文。中文译文标记“中”，英文译文标记“EN”。
- 两套完整离线模型随 ZIP 提供，不联网下载。英文连续正文仅保留于当前输入会话内存，不写入选词记忆。
- 开启原始隐私输入时暂停两种方向的处理。首次启用会校验两套模型，并运行中译英和英译中自检。

Tap Shift to switch between Pinyin and English translation. Spaces remain within the English sentence. Two independent preferences control Pinyin-to-English and English-to-Chinese default output. In English mode, Enter commits the default language and Tab commits the alternative; Esc keeps the English source. Both models are bundled, with no runtime download or cloud fallback. English prose is not added to candidate memory. Windows host interaction remains pending user testing.

## 0.1.4 选词记忆、翻页与复制 / Ranking, paging and copy

- 有拼音候选时，按 `=` 下一页、`-` 上一页，每页五项；PageUp / PageDown 仍可用。每次查询最多展示 100 个候选。第一页/最后一页不会循环；没有候选或按 Shift 输入 `+` / `_` 时保留正常符号输入。
- 默认开启常用短词排序：根据相同拼音下明确选定的短候选次数，把常选字词移到前面；不同拼音独立计数。最多四个汉字、2048 项；不会用常选单字顶掉拼音尚未输入完的完整长词/整句候选。排序只调整现有 Rime 候选，不另造候选。
- 设置中可关闭“记住常选短词”，也可“清除选词记忆”。普通桌面宿主通过 Windows 用户 DPAPI 加密保存到 `%LOCALAPPDATA%\FloatingTranslate\candidate-use.dat`。这属于新增的本地选词记录，**不再宣称完全零记录**；其中可能含人名或私人短语。没有启用云同步或上传。
- 系统搜索等 AppContainer 宿主不访问普通用户目录，只在当前进程内记忆；关闭该宿主后清空。关闭“记住常选短词”会停止应用和写入记录；已有记录需用清除按钮删除，沙箱内已有记忆需关闭宿主清空。读取/保存失败不会阻断输入。
- 复制按钮扩大点击区域；按下时固定本次待复制英文，不随鼠标改变。剪贴板暂时占用时最多异步尝试 12 次，每次间隔 80 毫秒；成功才显示绿色对勾，失败显示红色提示，悬停可看原因。关闭浮窗或译文改变会取消旧重试，避免过后复制过期文本。点击复制、拖选后 Ctrl+C 和右键复制共用此逻辑。

While composing, `=` / `-` move between five-item pages, with up to 100 candidates. Explicitly selected short Chinese candidates gain priority for the same spelling. Up to 2,048 entries of at most four CJK characters are stored using Windows user-scoped DPAPI; this is local selection data and can contain names or private phrases. Settings can disable or clear it. Sandboxed hosts retain session-only ranking. Copying now has a larger target, bounded asynchronous retries and success/error feedback; stale retries are cancelled when the panel closes or translation changes. Windows behavior still requires user verification.

## 0.1.3 外观调整 / Compact visual refinement

- 英文 16 → 14 DIP，中文候选 21 → 17 DIP，编号 10 → 9 DIP；单行译文卡从 44 → 34 DIP，常规双行浮窗从 94 → 74 DIP（约矮 21%）。窗口仍按系统 DPI 缩放。
- 收紧左右内边距与候选间距，最大宽度从 560 → 440 DIP。长译文、长候选继续完整换行，超屏幕高度继续滚动查看。
- 冷灰色轻微渐变、细描边、低对比分隔线、柔和蓝色选中态；复制图标缩小并加上鼠标悬停反馈。圆角外继续透明。
- 保留 0.1.2 的系统搜索兼容处理、默认下方显示和隐私规则。当前包的系统副本路径为 `%ProgramFiles%\FloatingTranslate\0.1.5`。

The English/candidate/number type sizes are reduced to 14/17/9 DIP. A typical two-row panel is 74 DIP tall instead of 94; a translation-only card is 34 instead of 44. Tighter spacing, a cooler subtle gradient, fine borders, quieter dividers, blue selection and a smaller copy icon refine the presentation. Text still wraps completely and follows system DPI. The 0.1.2 compatibility and privacy changes remain included. Visual results on Windows await user testing.

## 0.1.2 修复 / Fixes

- 加入 Windows 系统搜索所需的沉浸式/系统托盘能力注册。首次启用时，把包内组件复制到 `%ProgramFiles%\FloatingTranslate\0.1.2`，使受沙箱限制的宿主能读取词库、DLL 和模型；不下载组件、不放开用户桌面或 AppData 的权限。
- 拼音临时目录在 AppContainer 宿主中使用该宿主自己的 `TempState`。共享目录只保存四项非敏感开关，宿主仅可读取；读取失败保持隐私模式，不退回不受限制的访问。
- 默认改为光标下方；旧版的默认“上方”设置迁移一次，之后仍可手动选择。下方空间不足才翻到上方。
- 使用真实窗口圆角区域裁剪，移除矩形系统阴影，防止远程桌面把圆角外显示为白色直角。
- 沙箱不能写入设置时，Shift 中英切换在当前宿主内保留，避免被定时刷新立即切回。长期偏好请在设置程序中保存。

Version 0.1.2 adds immersive/tray registration and copies bundled read-only components into Program Files during activation. AppContainer hosts use their own temporary storage and can only read a dedicated directory containing four non-secret preferences. Inaccessible preferences fail closed. The popup defaults below the caret, migrates the old default once, and uses a rounded window region to exclude rectangular corners. Unsaved sandbox mode switches remain local to the current host.

按用户要求，本轮停止云电脑操作，交由用户实机验收。以下为代码层面修复与交叉编译检查，**没有把系统搜索输入、安装权限及远程透明效果标记为实机通过**。

At the user's request, interactive cloud-PC testing was stopped. System-search input, installation permissions and remote corner rendering still require user verification; compilation is not host validation.

## 0.1.1 修复 / Fixes

- 浮窗按内容宽度自适应，只有译文时收为一行小卡片；取消重复拼音行，采用紧凑灰色圆角、蓝色选词和复制图标。
- 候选按实际文字宽度排布，长词换行完整显示，不加省略号。整窗超过屏幕高度时可用滚轮或右侧细条查看，翻译也不截断为省略号。
- 修正 TSF 范围端点随新组合文字移动、以及宿主重复编辑通知导致整句清空的问题。临时不可用的范围保留本次输入的内存上下文，但不允许凭不可靠范围替换文档；确实移动光标、修改已跟踪文字、切换输入框或超时仍清理。

The popup now sizes to its content, with a compact translation-only card, a rounded neutral background, blue candidate selection and a copy icon. The duplicate Pinyin row is removed. Candidates use measured widths and wrap without ellipses; overflowing content can be scrolled. TSF endpoint gravity and repeated host edit notifications no longer discard the sentence merely because a range is temporarily unavailable. Unreliable ranges disable automatic replacement; confirmed movement, tracked-text edits, focus changes and expiry still clear context.

外观与交互以 Mac 版为参照；Windows 使用不同的原生绘制和翻译引擎，不承诺像素或译文完全相同。新版已通过核心逻辑与布局检查、x64 交叉编译及完整性审计；Windows 微信宿主复测待完成。

The presentation follows the Mac version. Native rendering and translation engines differ, so pixels and translations can differ. Core/layout checks, x64 cross-compilation and package audits are covered; actual Windows WeChat retesting remains pending.

## 解压与启用

1. 将整个压缩包解压到一个固定的本机目录，例如 `C:\Apps\FloatingTranslate`。首轮测试建议使用此目录。不要直接从压缩软件中运行，也不要放网络共享目录。
2. 双击“启用浮译.cmd”，按 Windows 提示批准管理员注册与本机组件复制。缺少 Microsoft Visual C++ x64 运行库时，会用包内微软签名的官方运行库安装程序准备，不下载组件。程序会先在独立进程验证真实拼音和离线翻译，通过后才注册，再由原用户启用自己的输入配置。
3. 按 Win + 空格，选择“浮译”。打开“浮译设置.cmd”，在试用框输入 `nihao`，按空格确认。第一次翻译需要校验并加载随包模型，后续在本进程内复用。
4. 其他应用已打开时可能需要保存草稿后重新打开，刷新输入服务连接。不要强制结束有未保存文字的软件。

**Windows 原生输入法不能只解压而完全零注册。** 这里的便携版指程序、词库、模型、推理库及必要运行库安装程序都随目录提供，不装 Python、Java、.NET 或开发工具，不另外下载语言包。首次启用需要一次系统注册，之后按系统输入法切换即可。新版注册的是 Program Files 中的副本；解压目录用于设置、更新和停用，请保留到测试结束。

目标为 Windows 10 / 11 x64 的桌面应用。此包只含 x64 TSF DLL；**32 位应用、Windows ARM64 原生应用尚未支持；系统搜索、UWP/沉浸式输入以及管理员应用兼容性仍待实机验收**。当前是未签名的开发试用版，不能当成已经在所有 Windows 软件中验证完成的稳定版。

## 与 Mac 一致的主要操作

- 轻按 Shift 切换中文拼音 / English 翻译模式；英文模式保留大小写与标点，Ctrl / Alt / Win 快捷键放行。
- 中文模式支持全拼、简拼（`nihao` / `nh`）、常见错拼（`zhogn` / `nihoa`）和音节分隔（`xi'an`），通过 Rime 原方案处理，不使用简化的临时字表，关闭 Rime 自动学习；0.1.4 的主动选词计数由独立加密模块处理。
- 空格、1–5 或鼠标点击选词；方向键移动候选，PageUp / PageDown 每次移动 5 项。
- 分段输入“我 / 今天 / 去北京”，翻译累计当前整句，不只翻译最后一段。不扫描文档前文。
- 设置“拼音转换后默认输出英文”后，先选词，再按空格或回车提交就绪译文；普通中文默认时 Tab 提交另一语言。替换已上屏整句只在 TSF 范围和光标相邻关系可验证时进行，否则提示复制英文。
- 英文译文可以鼠标拖选，Ctrl+C 或右键复制选中部分；“复制”按钮在有选中内容时复制选中部分，否则复制整句。
- 浮窗跟随 TSF 文本光标，可设置优先上方/下方，碰到屏幕边缘翻转并约束到当前显示器工作区。无法获得可信坐标时隐藏或保留同一输入上下文的有效坐标，不跳到鼠标位置。
- Esc 取消组合，退格编辑拼音或候选；句号、问号、感叹号结束当前句子。移动选区、外部编辑、快捷键、切换窗口/输入法会清理句子；未完句停顿 60 秒、句末 8 秒后清理。
- 隐私模式暂停拼音和翻译，原始按键交还应用。

## 离线与隐私

模型、分词器和词表随压缩包完整提供，所有模型先验证 SHA-256。推理只用 CPU，不调用 Google、微软、国内云翻译 API 或开发者服务器。应用代码没有 HTTP 客户端、遥测或自动更新；ONNX Runtime 遥测明确关闭。运行库由包内官方安装程序准备。系统的 UAC、证书检查、SmartScreen 或 Microsoft 运行库安装程序自身行为不等于应用上传正文；不能控制 Windows 和其他程序的网络行为。

不读取系统剪贴板，不收集复制历史，不保存连续输入正文或译文历史。开启选词记忆时会保存上述短候选及拼音、次数，不能把它描述为完全不保存输入相关数据。只有明确点击复制、拖选后 Ctrl+C 或右键复制时才写入系统剪贴板，系统自己的剪贴板历史和云同步由 Windows 设置决定。Android 的 24 小时剪贴板历史不属于本 Windows 首版功能，因为本次以 Mac 功能为目标。

启用前从旧 `%LOCALAPPDATA%\FloatingTranslate\settings.ini` 迁移设置；启用后设置保存到 `%PROGRAMDATA%\FloatingTranslate\Preferences\当前用户SID\settings.ini`，只含默认语言、位置、隐私、英文输入模式、选词记忆等开关，短词记录不会写入这个共享目录。该专用设置目录向 AppContainer 提供只读权限，不存放输入内容。普通宿主的 Rime 固定词库缓存在系统临时目录中的 `FuyiRime-进程号`，沙箱宿主则在自身 `TempState` 中创建同名缓存；用户词典和学习关闭。活动拼音、中文句子和译文只在内存中保留；不生成含真实输入的调试日志。

TSF 安全激活、输入框标记 Password/Private/NumericPassword/NumericPIN、标准 Win32 密码样式和手动隐私模式会停止处理。自定义或错误标记的密码框无法可靠识别，**输入密码、验证码和密钥前请手动切换系统键盘**。测试工具只用固定示例句，不读取用户文档。

当前支持中英双向翻译。OPUS-MT 的翻译质量与 Mac 的 Apple Translation 不同，口语、专名和复杂语境可能不准，不承诺相同译文。输入最多 256 个模型子词、输出最多 128 个子词，超长提示分句，不静默截断。

## 停用、更新与删除

先切到其他输入法，保存工作。双击“停用浮译.cmd”并批准注销。关闭使用过浮译的软件，确认文件不再占用后再删除目录。若仍占用，可正常注销 Windows 后删除；不强制杀浏览器。

更新时先停用旧版，再解压完整新版到固定目录并启用。注册组件位于 `%ProgramFiles%\FloatingTranslate\0.1.4`；停用只注销服务，不强制删除仍被宿主占用的组件。确认注销且文件不再占用后，可删除该版本目录及解压目录；保留设置便于以后重新启用。只注销本应用自己的 CLSID/Profile GUID，不移除其他系统输入法或键盘配置。不自动设置为默认输入法。

## 开发与验证

代码位于 `src/`，核心状态测试位于 `tests/core_test.cpp`。开发电脑需要 LLVM-MinGW 20261006 工具链和 Python 3；这些不需要装在最终用户电脑上。非 Windows 的开发主机还需要 Rime 1.17.0 的 rime_deployer 来预编译词库；Windows 开发时使用固定 SDK 内的工具。开发阶段下载固定版本依赖，下载文件的 SHA-256 记录在 `tools/dependencies.lock.json`；最终用户包内已经具备全部文件。

```sh
# 在仓库根目录；先准备 Android 共享的固定模型（只在开发电脑）：
python3 Android/tools/prepare_translation.py
# LLVM_MINGW 指向已解压的 llvm-mingw 工具链：
python3 -m pip install -r Android/tools/requirements-translation.txt
python3 Windows/tools/prepare.py
python3 Windows/tools/build.py
```

在 Windows 上运行“检查离线组件.cmd”可检查模型完整性、Rime 全拼/简拼/错拼和真实中英双向翻译，无需先注册输入法。`FuyiCoreTest.exe` 检查分句、过期、Shift 及多屏位置。测试结果与未完成的兼容验收请见仓库 `Windows/TESTING.md`。仅编译成功不能证明 Chrome、Word、Qt 编辑器等宿主的真实 TSF 兼容性。

中译英模型来自 Helsinki-NLP/opus-mt-zh-en / Xenova 固定转换，遵循 CC BY 4.0；新增英译中模型来自 Helsinki-NLP/opus-mt-en-zh / Xenova 固定转换，遵循 Apache 2.0；Rime BSD、词库 Apache 2.0、ONNX Runtime 及 Extensions MIT、LLVM-MinGW 及静态运行库保留各自许可。归属、改动说明、许可证和上游第三方声明均在 `licenses/`。Microsoft Visual C++ 官方安装程序带有独立条款，不被本仓库许可证覆盖。

参考：Microsoft TSF / ITfKeyEventSink / ITfContextView 文档，Rime 官方 Windows 实现 Weasel，以及仓库 Mac 版输入会话实现。

系统搜索兼容参考：[Microsoft IME requirements](https://learn.microsoft.com/en-us/windows/apps/develop/input/input-method-editor-requirements)。这里使用普通候选窗，未声明搜索面板内嵌候选的 UIless/Search contract。

复制实现参考：[OpenClipboard](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openclipboard)、[SetClipboardData](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setclipboarddata)。
