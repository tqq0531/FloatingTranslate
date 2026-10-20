# 输入法兼容性与参考实现

适用版本：0.5.11。核对时间：2026-10-20。

## 本次 Chrome 故障

现场现象：浮译退出、程序切到系统输入法后，Chrome 地址栏仍不能打字，其他软件正常；用户在 Chrome 中从系统输入法菜单手动选择 ABC 后恢复。未关闭网页、重启 Chrome 或读取网页内容。

这说明输入源显示与 Chrome 的实际输入会话状态可能不同步，不能只凭 `TISSelectInputSource` 返回成功断言恢复。没有复现出浏览器死锁，也没有证据证明拼音引擎卡死。保留这个区分，避免把“测试通过”写成“Chrome 全面兼容”。

## 开源参考

- [Rime 鼠须管的控制器](https://github.com/rime/squirrel/blob/master/sources/SquirrelInputController.swift)：核对激活时绑定客户端与键盘布局、快捷键放行、编辑文本缓存、结束会话时提交与清理的方式。
- [鼠须管的应用生命周期处理](https://github.com/rime/squirrel/blob/master/sources/SquirrelApplicationDelegate.swift) 及 [macOS 26 程序切走遗漏 deactivateServer 的问题 #1140](https://github.com/rime/squirrel/issues/1140)：参考输入源变更通知的清理兜底。
- [程序切入也可能失效的报告 #1162](https://github.com/rime/squirrel/issues/1162)：不把自动切入作为实际可用的验证依据；目前推荐系统菜单手动选择。
- [小麦注音的控制器](https://github.com/openvanilla/McBopomofo/blob/master/Source/InputMethodController.swift)：核对激活 / 结束状态边界，以及不在结束会话后额外发送空编辑文本的兼容处理。
- [Apple 的 IMKInputController 文档](https://developer.apple.com/documentation/inputmethodkit/imkinputcontroller) 与本机 SDK 头文件：核对事件掩码、文本范围和客户端调用约定。

本次依据上述行为设计自行实现，没有整份搬运这些项目的控制器。Rime 本体及随包依赖的许可证仍随应用分发。

## 已落实的处理

1. IMK 控制器只做系统消息转发；`InputSession` 管理组合、当前句子、翻译任务与客户端，便于直接测试输入流程。
2. 跟踪当前活动会话，切走输入源或目标应用退出前台时结束旧会话。重复的结束通知不会再次插入同一段文字；不相关应用的通知不会中断当前输入。
3. 只订阅键盘按下和修饰键变化；鼠标点出组合区交由系统处理，不接管浏览器鼠标事件。
4. 激活时指定系统 ABC 作为基础键盘布局。Command / Control / Option 快捷键放行；空闲时 Esc 放行。
5. 相同的编辑文本不重复发送；文本范围缺失不阻止正常中文上屏；无法核实整句替换范围时不替换无关正文。
6. 拼音引擎未加载时放行按键，不让用户陷入按键被吞掉但没有文字的状态。
7. 异步回调只更新当前会话；切走时取消翻译与定时器，清理浮窗和内存上下文。

## 人工验收

在 Chrome 中从系统菜单手动选浮译，分别在地址栏、普通 textarea、可编辑聊天输入框验证。只用虚构文字。

- 分别输入 `wo`、`jintian`、`qubeijing`，每段空格确认；应得到“我今天去北京”，英文根据整句更新。
- 轻按 Shift 后直接输入 `abc`，再按 Shift 切回中文。
- 输入一半时按 Command+L，应转到地址栏；输入应继续正常。
- 拼音编辑中手动切 ABC、切回浮译；旧文字不能重复插入，旧浮窗不能残留。
- 切换标签页、应用、移动光标后开始新句，不应串入上一处的上下文。

自动测试覆盖会话逻辑和本机翻译；尚未代替上述各类真实网页输入控件的人工验证。

## 0.5.3 Chrome 光标查询与英文复制

进一步参考 [鼠须管尚未合并的 Chrome 停顿修复 PR #1154](https://github.com/rime/squirrel/pull/1154)。它指出 Chrome 同步查询光标位置可能等待渲染进程；这是本次需要排除的代码路径，并非已经证明用户故障只由它引起。

浮译针对 Chrome / Chromium 禁用文档选区和 markedRange 查询；坐标查询统一放在下一轮主循环，按组合复用有效坐标并限频。失效定位请求会被新会话取消，不再以 0.1 秒定时器反复触发同步定位。代价是组合中换行时浮窗可能稍后才更新位置；没有可靠位置时继续不使用鼠标坐标猜测。

英文改成可选择的只读 NSTextView；非激活浮窗仅在用户选择文本时获得键盘焦点，支持复制选区、右键菜单和整句按钮。选择期间冻结显示；输入会话结束时最多暂存可见译文 15 秒，安全输入和隐私模式仍立即禁止复制并清理。

测试页面：`Tests/chrome-input-check.html`，无脚本、无网络请求，用于人工测试普通 textarea 和 contenteditable。自动会话测试不代替真实 Chrome 的键盘验收。

## 0.5.5 输入服务初始化与浏览器连接

实机对比中，原 Chrome 进程选中浮译后没有把按键交给控制器，独立 Chrome 进程能够触发控制器并输入“你好”。这是旧连接仍未重建的证据，不能把“进程存活”当作输入服务可用。

与鼠须管的 Main.swift 启动顺序对齐：在创建 NSApplication 之前建立唯一的 IMKServer，并显式解析控制器类；控制器显式调用系统初始化器、缓存真实客户端，在事件 sender 不可用时使用系统客户端兜底。临时方法调用诊断已从发布源码删除，不记录用户按键或输入文本。

更新时应先通过系统菜单切 ABC 并结束当前组合。旧的 Chrome 进程若仍不能输入，需用户先保存网页草稿，再正常退出 / 重开浏览器。安装程序不会擅自关闭浏览器、注销会话或更改浏览器安全设置。

只读浮窗没有常规应用的“编辑”主菜单，因此显式处理 Command+C / Command+A，并保留右键复制。实机验收分别检查输入、整句按钮复制、鼠标选择和快捷键复制，不只检查视图的 selectable 属性。

## Live acceptance: 2026-10-20 / 0.5.5

Environment: macOS 26.5.1, Chrome 155.0.8059.85, Apple Silicon. With explicit user approval after saving drafts, the original Chrome exited normally and reopened using --restore-last-session. The isolated test Chrome was closed. Apple Events JavaScript permission was not enabled.

Verified in the original Chrome profile and local fixture:

- Separate wo / jintian / qubeijing commits produced the complete Chinese sentence.
- The floating panel rendered: I'm going to Beijing today.
- The whole-sentence copy button copied that English into the fixture's contenteditable area.
- Mouse dragging visibly selected the floating English; Command+C and paste produced the selected text in the fixture.
- Only fixed test text was used. The driver checked clipboard changeCount before pasting, without reading previous clipboard contents.

Evidence: build/acceptance-chrome.png and build/acceptance-selection.png. This validates the local textarea, floating text selection, and contenteditable paste. It does not establish compatibility with every web editor. Temporary event-delivery diagnostics were removed from release sources.

## 0.5.6 输入法图标

参考 [青简的输入模式配置](https://github.com/qingjian-team/qingjian/blob/main/apps/macos/Info.plist) 和 [图标尺寸说明](https://github.com/qingjian-team/qingjian/blob/main/docs/design/architecture.md)，改用 22×16 点的 PDF 圆角字牌。字形按实际轮廓居中，不依赖字体基线或 TIFF 分辨率解释。菜单、选中状态和切换面板指定同一份图标；保留系统模板着色。

增加独立的 `.Hans` 输入模式及中英文名称；输入会话识别旧输入源和新模式，避免升级后把自身模式切换当作离开浮译。macOS 会缓存输入法图标；仅复制安装包不一定即时刷新。现场刷新 TextInputMenuAgent 后，菜单栏与输入法列表已采用新字牌，并再次验证 Chrome 本地测试页能输入“我今天去北京”。重启系统切换提示服务并重新聚焦测试页后，实测光标旁蓝色切换提示中的“译”字牌也已居中，字号不再超出提示区域。截图保存在 build/acceptance-icon-menu.png、build/acceptance-icon-menubar.png 和 build/acceptance-icon-switch.png。

## 0.5.8 旧版排查记录（结论已由 0.5.9 更正）

实机中，系统设置列出浮译，TIS 也报告父输入法与 `.Hans` 模式 enabled=true；但 com.apple.HIToolbox 的 AppleEnabledInputSources 中没有浮译，实际菜单只有 ABC 与简体拼音。点击“完成”、重新注册、TIS 重新启用和仅刷新菜单均未恢复，因此不能把 enabled=true 当作菜单可用的证据。

参考 [Meltype 的同类实机报告 #21](https://github.com/yksr-melt/Meltype/issues/21) 核对登记结构，自行实现仅追加浮译身份的修复：安装按钮显式调用 --repair-menu，先将原启用清单备份到 ~/Library/Application Support/FloatingTranslate/InstallBackups，再补缺失的 Keyboard Input Method 父项与 Input Mode 中文模式项，保留其他输入源及其顺序。已经存在时不重复追加、不重写。同步后重新读取核实，刷新 TextInputMenuAgent；不调用 TISSelectInputSource，不重启浏览器。

注册检查现在分别报告独立模式可选择性和 MENU_PERSISTED，默认注册 / --check 都只检查，不修改启用清单。--repair-menu 仅接受已安装的浮译路径。备份只包含输入源标识与配置，不包含输入文字。

本次现场结果：已安装 0.5.8，缺失的两项持久登记已补齐并读回验证，原有输入源仍保留；合并逻辑自测与隐私检查通过。但当前登录会话即使经系统设置重新添加、重新注册、刷新菜单与切换提示服务，切换菜单仍只有 ABC / 简体拼音，尚未完成修复后实际输入验收。需要用户保存文稿后注销并重新登录，继续验证；不能将此次登记成功写成菜单已经恢复。不自动注销用户，也不更改浏览器设置。

## 0.5.9 恢复切换与输入：2026-10-20

更正 0.5.8 的诊断：本机第三方输入法已保存在 `com.apple.inputsources / AppleEnabledThirdPartyInputSources`；只查 HIToolbox 旧清单不能判定“未添加”。安装检查与修复现在选择当前系统使用的清单；合并函数保持其他输入源与顺序，重复执行不追加同一身份。0.5.8 实验补入旧清单的浮译副本已清理，原始清单备份保留。

输入服务连接名与启动代码统一使用 `com.fuyi.inputmethod.FloatingTranslate_Connection`，启动时验证配置，服务建立失败不会留下看似存活的空实例。参考 [VietTelex 的客户端连接与多实例问题记录](https://github.com/ptrinh/viettelex/blob/main/docs/MACOS_IME_NOTES.md) 和 [IMKServer 初始化约定](https://developer.apple.com/documentation/inputmethodkit/imkserver/init(name:bundleidentifier:))；使用背景输入服务身份。单实例检查在建立 IMKServer 之前进行，打开另一份应用时转到已有实例的设置。安装备份改用 `.app.backup`，避免把备份当成可运行应用再次登记；本机历史备份仅解除应用登记并改后缀，文件均保留。

新的 `FuyiInputSetup.app` 是独立的普通文本客户端，通过自己的 NSTextInputContext 选择浮译，避免用外部 TISSelectInputSource 返回值冒充实际激活。安装后打开启用窗口，设置和状态菜单也提供“启用并试用”。它不包含按键模拟或辅助功能代码。输入控制器临时诊断只用于定位消息流，最终源码已移除，不记录用户键码或输入内容。

实机验收：macOS 26.5.1，Chrome 155.0.8059.85，原 Chrome 进程 PID 32328 保持运行。没有注销、重启电脑或重启浏览器。

- 系统菜单恢复为一个浮译项目，并能从 ABC / 简体拼音切到浮译。
- 随包启用窗口中 `nihao` + 空格得到“你好”。
- 原 Chrome 的本地测试页依次输入 `wo` / `jintian` / `qubeijing` 并分别确认，得到“我今天去北京”。
- 浮窗显示真实本机翻译 `I'm going to Beijing today.`；拖选英文、Command+C 后粘贴到测试页 contenteditable，文字一致。只检查剪贴板版本变化，不读取原有剪贴板内容。
- 轻按 Shift，输入 `abc`，再轻按 Shift 输入 `nihao` + 空格，结果为 `abc你好`。
- 核对了原生客户端、Chrome 普通 textarea 与 contenteditable，未据此声称所有应用和所有网页编辑器均兼容。

截图：`build/acceptance-0.5.9-menu.png`、`build/acceptance-0.5.9-chrome.png`、`build/acceptance-0.5.9-shift.png`。引擎、本机翻译、隐私、输入会话和浮窗复制测试通过；登记合并自测覆盖新版/旧版清单选择、其他输入源保留和重复执行。下一次升级仍应先结束当前组合，再安装。

0.5.9 最终安装构建也验证了单实例：直接打开构建副本的设置入口后，仍只有一个输入服务，路径为已安装应用。最终 App / 启用辅助应用的签名与配置检查通过。


## 0.5.10 浮窗四角透明：2026-10-20

候选窗使用透明容器，并对毛玻璃材质设置圆角遮罩，避免只设置图层圆角后仍露出矩形背景。材质遮罩随窗口尺寸更新。

实际窗口截图验收覆盖浅色、深色，以及有候选词和只有译文的两种尺寸：四角像素 alpha 小于 0.02，窗口正文仍可见。整句翻译、鼠标选择、整句/部分复制及关闭清理测试通过。截图位于 `build/panel-NSAppearanceNameAqua-candidates.png` 等四张 `panel-*.png`。


## 0.5.11 Notepad-- 分段整句翻译：2026-10-20

本机 Notepad--（Qt，标识 `www.itdp.cn`）在选词上屏后返回负数光标位置与非零选区长度，例如 `{-3, 3}`，使旧逻辑误判为选区变化并清空整句。现在统一校验文档范围：负数、未知长度和加法溢出均视为范围不可用，保留本输入法刚刚输入的当前句子，但撤销可用于整句替换的位置记录。无需读取编辑器前后文。

输入会话回归测试覆盖负数范围、分段确认及定时跟踪、当前候选预览合并、真实本机整句翻译、禁止不安全替换、导航清理，以及不读取周围文本。

已安装 0.5.11，并在本机 Notepad-- 的独立测试文件中通过真实键盘事件验证：依次确认“我”“今天”“去北京”，上屏后保留整句英文；依次确认“为什么”“不”“一起”，继续输入未确认的 `fanyi`，仍预览“为什么不一起翻译”的整句英文。截图：`build/acceptance-0.5.11-notepad.png`、`build/acceptance-0.5.11-notepad-candidate.png`。测试没有修改用户原有文档。
