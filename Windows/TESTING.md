# Windows 0.1.10 测试边界与验收清单

## 2026-10-25 Windows 本机验证

- 在 Windows x64 使用固定 LLVM-MinGW 20261006 工具链原生构建；运行 FuyiCoreTest、FuyiCheck 和 tests/runtime_regression.cpp。
- 核心回归覆盖句首/句中/句尾插入及英文插词、快输后先删除尾部拼音再撤回选词、已提交文字交还宿主删除、延迟候选选词记忆排序和分页。
- 对 nihao、nh、zhogn、xi'an、nihoa 以及两个长拼音，逐项对比所有候选文本和消耗长度，延迟计算与完整计算一致；真实中英双向推理、取消和模型校验通过。
- 同机固定两组逐字符拼音输入（不含 UI/模型推理）：完整计算约 942.6 ms，按需计算约 41.9 ms。模型首次短句约 1065 ms，加载后两个例句约 600/641 ms；这是固定样例结果，不代表所有电脑或长句的延迟。
- 原生电脑控制能读取记事本焦点，但截图报 FrameArrived timeout，点击报 coordinate input geometry is unavailable。未将这些自动测试当作实际宿主交互及视觉验收。

依赖准备好后，在 Windows 仓库根目录运行 `python Windows/tools/build.py --test` 可重现上述原生测试，再运行 `python Windows/tools/audit_package.py Windows/build/FloatingTranslate-Windows-0.1.10-x64-offline.zip` 审计发行包。测试只使用固定句子，不注册输入法、不读取用户文档。

仍需交互复测：在记事本和微信输入“我去北京”，回到“我”后输入“今天”，应预览“我今天去北京”的整句译文；在句首、句末重复，并试 Backspace/Delete、选区和换框。开启默认译文输出时，检查中间插字后提交整句译文不会遗留原句尾部。快速输入拼音后退格逐字删除尾部拼音，再退回最近选词。100%、125%、150%、200% 缩放及跨屏检查外框、选中框、光标跟随、复制和长文本滚动。范围不可验证时应拒绝自动替换并提示复制。

日期：2026-10-23。当前开发主机为 macOS / Apple Silicon，交叉编译 Windows x64。**尚未在真实 Windows 系统完成输入服务与宿主应用验收。** 此包供指定测试人员试用，不应按“与 Mac 一样成熟稳定”对外宣传。


## 0.1.7 开发验证与用户验收 / Checks and host validation

本机已实际运行同一份词典纠错和语法实现：`I wnat go to teh gym now ` → `I want to go to the gym now `；正确示例、URL、代码样式和未完成词保持。核心检查覆盖隐私/非英文/文字改变/光标移动/前缀改变时拒绝保留旧组合。共享模型实际输出 `I want go to gym now` 及修正后的整句 → `我现在要去健身房`。

纠错区域采用与原生窗口共用的纯布局实现，在 100%、125%、150%、200%、250% DPI 检查建议文字完整换行、采用/复制按钮不重叠且在边界内。该检查不代表 Windows 原生绘制已验收。

搜索候选窗新增宿主 owner 和 IME 显示/隐藏/变化事件；Windows 搜索面板的轻关闭与焦点行为也待用户验收。

本轮编译四个 x64 二进制，审计 ZIP、全部文件、双向模型、词表校验、PE 导出与依赖。没有 Windows 可执行文件或系统搜索实机通过结论；代码和核心策略检查不能验证 TSF 宿主。

Windows 用户重点验收：

1. 保存编辑文字，切到微软拼音，停用旧版；启用完整 0.1.7，运行刷新Windows搜索.cmd，再打开搜索并 Win+空格选浮译。输入 nihao 空格得到“你好”；用检查搜索框权限.cmd 核对已安装副本读权限。脚本会重启 SearchHost/SearchApp/SearchUI，当前搜索查询会关闭。
2. 英文默认原文：输入 I want go to，回车，继续空格 gym now，检查译文使用整句。整句一次输入、宿主结束组合再追加、退格、句号和二次回车换行分别检查。
3. 修改前缀、移走光标、换框、进入隐私/密码输入后不能把旧句拼入新句，不得替换无关正文。未知范围可只预览，采用建议要求可信范围。
4. 观察基础纠错行，顶部复制为中文，复制建议为英文。采用后重新翻译；未采用时原文保持。译文或输入框变化后不得采用/复制过期建议；剪贴板繁忙仍重试并反馈。
5. 断网重复检查。不弹出在线纠错/下载请求；专名、真实词误用、复杂时态仍可能漏报/误报。手动核对建议，不要用真实密码测试。

Portable tests verify dictionary-backed edits, bounded common grammar rules, conservative skips and English-prefix retention policy. Shared inference tests verify both model directions and the complete reported gym sentence. Windows Search, ACL mutation, TSF range semantics and native correction controls remain user-test items. This is a basic offline assistant, not a complete grammar checker.

## 0.1.5 双向翻译验收 / Bidirectional translation

2026-10-23：核心回归、共享 C++ 双向实际推理（含取消/超长拒绝）、x64 四个二进制交叉编译、ZIP CRC/64 个文件 SHA-256/10 个模型文件/PE 导出与依赖审计均通过。Windows 输入与剪贴板实机行为仍未验收。

On 2026-10-23, core regressions, actual shared C++ inference in both directions with cancellation/length checks, four x64 cross-compiled binaries, ZIP CRC, 64 file hashes, 10 model files and PE/dependency audits passed. Actual Windows typing and clipboard behavior remain pending.

共享 C++ 推理代码使用固定英译中模型与简体目标前缀，在 Mac 开发机实际得到 `Have you worked out today?` → `你今天有锻炼吗?`。这不是 Windows 宿主验收。模型翻译仍可能不准确或重复词句。

Windows 待用户检查：

1. 两个默认输出开关四种组合分别保存、重开设置，确认独立生效。
2. Shift 进入英文，逐词输入整句，空格不提交；大小写、Caps Lock、数字和英文标点保留；回车默认语言、Tab 另一语言，Esc 保留原文。
3. 正在翻译时确认不能提交旧译文；切换模式或输入框不显示上次方向的结果。中文模式原有分段输入、选词记忆、翻页继续工作。
4. 点击复制中文、拖选后 Ctrl+C，粘贴检查内容；隐私模式和正确标记密码框不翻译、不记忆。
5. 断网启用与运行；模型缺失或损坏应自检失败，不能悄悄联网下载。

Both direction defaults are independent. Test sentence spaces, capitalization, punctuation, Enter/Tab/Esc, stale-result rejection, source switching, Chinese copy, privacy/password bypass, and offline activation. Shared inference, core tests and static package checks do not replace actual Windows typing tests.

## 0.1.4 回归与待测 / Ranking and copy verification

核心测试新增：相同拼音选词计数排序、候选消费长度保留、不同拼音隔离、长句/数字/英文排除、短词不能顶掉完整长句候选、计数序列化结构恢复及翻页边界。核心/布局测试在开发主机运行，原生 DLL 和设置程序交叉编译；DPAPI、跨进程文件锁、Rime 迭代和原生剪贴板仍待 Windows 实机验证。

测试者操作：

1. 输入 `shi`，用数字键或鼠标选“事”数次，再输入 `shi`，应优先“事”。换一个普通软件检查排序；关闭重开软件后应保留本机计数。长候选与普通英文不能被保存为记忆项。
2. 设置关闭记忆后应恢复 Rime 原始排序；重新开启后保留已有计数。清除后应恢复原排序。DPAPI 文件不是明文；本机用户身份下的其他程序仍可能调用解密，不能承诺能对抗本机恶意软件。
3. `=` / `-`、PageDown / PageUp 翻页后，1—5 应选择当前页对应候选。第一页与末页不越界；没有拼音时应正常输入 `=`、`-`、`+`、`_`。
4. 点击复制按钮后出现绿色对勾，再到本机记事本粘贴，核对英文完整且不是旧译文；拖选只复制选中部分。云电脑请区分远端本机复制与远端到 Mac 的剪贴板同步。
5. 若剪贴板被占用应重试，最终失败显示红色图标；悬停查看原因。译文变化、切换输入框或关闭浮窗后不得再写入旧待复制内容。空译文不应清空剪贴板。
6. 隐私模式和识别出的密码框不记录选词；关闭学习时不应用旧排序。系统搜索等沙箱只记当前进程，不能越权读取普通用户选词文件。

Pure-core tests cover ranking, consumed-prefix preservation, rejected inputs, phrase protection, round-tripping entries and page boundaries. Windows DPAPI, file locking, Rime iteration and clipboard writes remain unverified on Windows; the user requested no further cloud-PC operation.

## 0.1.3 外观检查 / Visual refinement checks

常规双行高度 94 → 74 DIP、单行高度 44 → 34 DIP；正文与中文候选分别为 14 / 17 DIP。复用同一份布局代码检查完整候选、长句换行、屏幕范围及 100%—250% DPI；原生绘制交叉编译并进行包完整性检查。按用户要求不再操作云电脑，字体观感、渐变、透明边缘和复制悬停效果交由用户实机测试，不将代码检查当作 Windows 渲染验收。

Regular two-row height changes from 94 to 74 DIP and translation-only height from 44 to 34. Existing layout regressions cover full text, wrapping, bounds and five DPI settings. Native drawing is cross-compiled and the full package audited; actual Windows appearance remains unverified at the user's request.

## 0.1.2 本轮状态 / Current status

用户报告 0.1.1 在 Windows 任务栏搜索中只有字母、浮窗在上方且圆角外有白色区域。云电脑初步操作受到输入法切换和快捷键转发影响，不能作为可靠的浮译验收结果；随后用户明确要求停止实机测试，由用户自行验收。

本轮修改：Program Files 只读组件副本、沉浸式/托盘能力注册、宿主 AppContainer 临时目录、四项开关的独立只读共享目录、设置读取失败保持隐私、沙箱中英切换的内存回退、下方默认及一次迁移、真实圆角窗口区域。未修改 Windows 安全隔离、杀毒或 SmartScreen。

Current changes address sandbox-readable runtime files, immersive/tray registration, host-owned temporary storage, read-only shared preferences, fail-closed privacy, local sandbox mode switching, below-caret placement and rounded window regions. Interactive cloud-PC testing was stopped at the user's request; the following checks remain pending.

用户重点验收：

1. 先切到微软拼音，停用旧版；关闭使用过浮译的软件后启用当前版本。管理员提示后应出现成功通知；失败时不要继续安装其他组件或关闭安全保护。
2. 重新打开任务栏搜索，Win+空格确认选择浮译，输入 `nihao` 并空格，必须得到“你好”；再在开始菜单搜索、Chrome、设置试用框中对比。检查当前不是本地 Mac 输入法转发。
3. 搜索框内轻按 Shift，输入 `abc`，等一秒后仍应为英文模式，再切回拼音。
4. 普通屏幕中部输入框默认浮窗在下方，屏幕底部空间不足时允许上翻；设置“下方”取消后应优先在上方。
5. 深色、浅色桌面与云电脑中检查四个圆角外都是背景，没有白色直角；125%/150%/200% 缩放复查。
6. 在设置中启用隐私模式后，普通软件和系统搜索均不应出现拼音与翻译；不要用真实密码测试。
7. 标准用户凭另一管理员身份通过 UAC 的安装、重复启用、设置迁移及注销需要单独验证。

## 0.1.1 回归 / Regression checks

- 核心测试加入“你好 / 今天 / 健身 / 了吗”的内存上下文与连续宿主通知策略；临时未知范围不等于已移动光标，已确认范围变化必须清理。TSF 分支的真实宿主行为仍需 Windows 复测，此测试不能替代微信输入验收。
- 同一份原生浮窗布局代码检查 100%、125%、150%、200%、250% DPI：短译文小卡片、紧凑候选、长句换行、超长候选全部字符保留、可滚动高度和屏幕宽度约束。
- 新增固定离线自检句“你好今天健身了吗？”。Windows 自检实际输出由测试者在“检查离线组件.cmd”中查看，不将交叉编译当作 Windows 推理结果。

- 本轮在 Mac 开发主机实际运行相同 C++ 推理实现及最终包模型：`你好，今天健身了吗？` → `Hello. Did you work out today?`；`你好今天健身了吗` → `You're working out today?`。无标点时模型可能省略问候语，这属于模型质量限制，不能将整句上下文修复宣传为与 Apple 翻译质量相同。没有把测试用的其他解码方案加入发行代码。

Shared core/layout regressions cover retained sentence context, unavailable-versus-moved range evidence, compact cards, complete wrapped candidates and bounded scrolling at five DPI settings. These are not actual TSF host tests. The offline component checker includes the reported workout sentence.

### Windows 微信复测 / Windows WeChat retest

1. 在同一输入框依次输入并确认 `nihao`、`jintian`、`jianshen`、`lema`，观察翻译随整句更新，而不是依次只剩 Hello、Today、Physical fitness。
2. 一次输入 `nihaojintianjianshenlema`，确认首候选完整显示；再试较长候选与屏幕右/下边缘。
3. 移动光标、选中文字、撤销、清空/发送消息后再输入，不应把上一句拼入下一句或替换无关正文。范围不可验证时应要求复制英文。
4. 在 125%、150%、200% 缩放下检查短句浮窗宽度、透明圆角、复制图标与长文本滚动。译文可拖选后 Ctrl+C，候选换行后鼠标点击应选中对应编号。

## 已完成的检查

- LLVM-MinGW 20261006 交叉编译原生 x64 TSF DLL、设置/启用/停用程序、离线组件检查程序以及核心状态测试程序；均不需要用户安装 Python、Java、.NET 或开发工具。
- 在开发主机实际运行同一份 C++ 输入状态核心，检查整句累计、60 秒/8 秒过期、光标移动清理、Shift 单击和 Shift 组合键，以及负坐标副屏/屏幕边界位置规则。
- 将推理实现独立为共享 C++ `TranslationModel`，在开发主机使用同版本 ONNX Runtime 1.23.2、Extensions 0.14.0 和最终包的同一模型/二进制词表运行真实推理，得到 `Hello.`、`I went to Beijing today.`、`Why don't you translate it together?`；取消旧请求及超长拒绝通过。**这不是 Windows 二进制执行结果**，用于验证共享模型和推理代码。
- 用 Rime 1.17.0 在开发阶段重新编译固定方案和词库，完整产物随目录提供，不依赖忽略的旧缓存。Windows 运行时采用官方 clang x64 Rime DLL；已排除会缺少 MSYS2 DLL 的 MinGW Rime 构建。
- 静态审计最终 PE 导出/导入和包内 SHA-256，检查 TSF 四个标准导出、x64 架构、直接/传递原生 DLL 依赖、模型及词表完整性。官方 VC++ x64 离线安装程序随包提供，解决 ONNX 运行时的 VC++ 依赖。
- 启用程序先在独立进程运行 `FuyiCheck.exe` 检查真实全拼、简拼、错拼和中译英，通过后才注册，失败不把新输入服务注册到用户应用中。管理员注册与原用户配置启用分开完成，只处理本应用自己的 CLSID/Profile GUID。
- 无全局键盘钩子、HTTP 客户端、云翻译、自动更新或剪贴板读取；ONNX 遥测显式关闭。键盘编辑访问使用 TSF 同步锁，无法取得锁时放行原键，不把未处理快捷键异步吞掉。

## Windows 测试人员操作

1. 完整解压；先在不含私人信息的普通文本框测试。双击“启用浮译.cmd”，批准管理员注册，等待自检完成，再用 Win+空格选浮译。
2. 关闭 Wi-Fi/网线，运行“检查离线组件.cmd”。应通过核心状态、模型校验、Rime 和真实翻译。保存的不是用户输入，只显示固定测试句。
3. 在 Windows 记事本、Chrome 地址栏/网页输入框、Edge、Word 64 位和常用 Qt 编辑器分别输入 `nihao`、`nh`、`zhogn`、`nihoa`、`xi'an`；空格、数字、方向键和鼠标选词，确认中文和拼音消费数量正确。
4. 分三次确认“我 / 今天 / 去北京”，观察译文仍为整句。默认英文选词后按空格提交，Tab 切换本次提交语言。移动光标或在句子中间编辑后，不应替换与该输入会话无关的文字。
5. 轻按 Shift 后直接输入 `abc`；Shift+A、Ctrl+L/C/V、Alt+Tab、Win 快捷键均不应被吞，切走再回来应继续正常输入。
6. 鼠标拖选英文并 Ctrl+C/右键复制，检查选中内容；无选中时复制整句。不要用私人剪贴板或密码测试。
7. 100% / 150% / 200% 缩放、负坐标副屏、屏幕边缘、编辑框滚动时检查浮窗位置和透明圆角。文本位置不可用时应隐藏/保持本上下文可信坐标，不跳到鼠标。
8. 在标准密码框输入虚构字符串，应直接交给应用、不出现拼音和翻译。手动隐私模式也应关闭翻译。自定义密码框无法可靠识别，真实密码仍使用系统键盘。
9. 切回系统输入法后停用，关闭使用过浮译的程序，再删除目录。确认其他输入法和键盘配置保留。

## 仍未验收/支持的范围

- 真实 Windows 10/11 上的注册、Win+空格列表、Chrome/Word/Qt 的 TSF 时序，以及窗口焦点、鼠标复制、高 DPI/副屏和标准密码框。
- 32 位宿主程序、Windows ARM64 原生程序和安全桌面不支持；系统搜索/UWP/沉浸式宿主及管理员程序尚未实机验收。
- 分发包未做商业代码签名；不要求测试人员关闭杀毒、安全启动或 SmartScreen 来绕过错误。文件若被阻止或隔离，应停止并报告。
- 翻译模型与 Apple Translation 不同，不保证与 Mac 产生相同文本。便携输入法首次仍需系统注册，0.1.2 注册 Program Files 中的副本，停用不强删占用中的版本目录。
