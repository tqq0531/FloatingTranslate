## Android 0.1.10 — 2026-10-29

拼音栏使用明确的内容宽度和填充视口，修复仅显示首字母；新增可滚动的三列候选展开面板。中文组词期间输入标点先确认中文，进入数字/符号页保留组词；有拼音时操作键原样上屏，下一次再执行宿主动作。空格继续选中文，译文通过“用英文”提交。

Build, lint and 15 JVM tests passed. Focused Pixel_6 API 36 device check passed: yizhi is shown in full; expand and choose 一直; nihao plus comma produces 你好，; codex plus action commits the literal input. Earlier device attempts were blocked by emulator lock screen, then corrected a test selector and allowed asynchronous dictionary startup. Final focused run: OK (1 test), 8.835 seconds. No physical phone installation was performed.

## Android 0.1.9 — 2026-10-29

长串拼音增加整串容错候选，支持混合简拼、末尾补全和相邻字母交换；保留分段候选及其原始输入消耗长度。无法识别的字母保留，不静默丢弃。修复词库异步加载期间忽略最初按键的问题。

Full-span fallback candidates support mixed initials, trailing completion and adjacent transposition. Partial candidates remain available; unknown letters are retained. Keystrokes are preserved during dictionary loading. This dictionary-based decoder does not guarantee intended meaning or WeChat-level prediction.

Build, lint and all 15 JVM tests passed. One focused Pixel_6 API 36 instrumentation test passed: type nihaojintianzhogn and commit the full Chinese candidate; type nihaoajintianhiodezy, choose 你好, verify the exact remaining Pinyin, then commit the remainder. Gradle's offline device runner lacked a cached UTP dependency, so APKs were installed and the same instrumentation test executed directly through adb.

## Android 0.1.8 — 2026-10-28

Build, lint and 14 JVM tests passed. On the Mac-hosted Pixel_6 API 36 emulator, the combined run passed 19 of 20 device checks. The remaining real-model test exceeded its old 5-second wait; after extending that test wait to 30 seconds, it passed. A final targeted run passed all three checks: real-model default output/copy, long translation scrolling plus numeric/symbol layout, and encrypted personal dictionary persistence. Across these runs all 21 distinct device checks passed; this is not a claim of physical-phone coverage.

验证内容 / Checks:
- 选过“时”后，下一次 shi 的空格首选为“时”；记忆可清空、从加密文件重载，并生成相邻选词联想。/ Choice ranking, clearing, encrypted reload, and next-word associations.
- 本次拼音达到 80 个字母后拒绝继续追加并提示先选词；整段可左右滑动，确认后可以继续输入。/ Explicit composition cap with horizontal scrolling and continuation after commit.
- 真实长句译文保持单行并能左右滑动；123 数字键盘和独立符号入口正常。/ Real long translation scrolls horizontally; number/symbol controls work.
- 现有高度、跟随系统主题、复制历史和输入流程回归。/ Existing appearance, clipboard and typing regression checks.

Screenshot: [long translation after scrolling](docs/keyboard-018-translation.png). The test screenshot temporarily permits capture in the test activity; production privacy flags remain. Local learning is bounded word-level memory, not a complete predictive language model.

## Android 0.1.7 — 2026-10-27

Build, lint and 12 JVM tests passed. Pixel_6 API 36 emulator: 17 of 18 device checks passed in the combined run; the remaining letter-preview check was corrected to inspect pixels (non-touchable popup windows do not expose accessibility nodes) and passed independently. All 18 checks have passed on the same production APK. Cold-start instrumentation now rebinds the IME after an Android 16 client-visibility rejection; production input behavior was not bypassed.

New checks: `d` → 的; `w` → 我; `zh` contains 中; full/initial mixed input; extension dictionary loading; switching basic/extended during input; unchanged key position while candidates appear; press preview above its key and dismissed on release. Existing checks cover translation, clipboard, height and live system light/dark changes.

Screenshots: `docs/keyboard-017-light.png`, `docs/keyboard-017-dark.png`, `docs/keyboard-017-press.png`. Physical-phone typing feel still needs user feedback; the dictionary matcher is not a complete native Rime/Fcitx engine.

# Android 0.1.6 验证 / Validation

在 Mac 的 Pixel 6 Android 16 模拟器（4 KB 页）完成本次验证。安装包内置中英双向模型，测试 APK 不会发给用户。

Validated on a Pixel 6 Android 16 emulator running on macOS (4 KB pages). Both translation directions are bundled; the instrumentation APK is not distributed to users.

- 10 项 JVM 测试：真实拼音词库、简拼、24 小时到期边界、去重不续期、容量限制和模型进度。
- 16 项键盘/设置/剪贴板设备测试全部通过（关闭模拟器 Wi-Fi 和移动数据后重跑）：错位按键几何、触摸间隙、长按退格、Shift、数字符号、中文分段整句、英文空格后继续整句、原文与译文提交、发送动作、敏感字段隔离、历史加密/过期/插入/删除/清空。
- 外观设备测试：44/54/68 dp 按键高度变化；键盘内切深色与调高度保留 `hello world` 草稿；三种主题设置持久化；真实 `cmd uimode night no → yes → no` 下截图像素验证白键转深灰再转白。
- 1 项独立真实模型测试通过（内含双向多条句子、取消与模型切换；Wi-Fi/移动数据关闭）：`你好 → Hello.`；`I want to go to the gym now → 我现在想去健身房`；`The cat is sleeping on the sofa. → 猫睡在沙发上`；`Hi my name is Jack → 嗨,我叫杰克。`，另测中译英整句、hi、取消、方向切回及双方初始化。
- Debug APK/测试 APK、JUnit、Android Lint 构建通过；Lint 无错误，保留原生控件、中文硬编码和旧 API 等建议。两套模型都由构建任务按 SHA-256 和大小验证。
- 截图来自实际模拟器，测试仅对测试用 Activity 暂时关闭防截图标志；正式设置页保持该保护。模拟器可见窗口已在 Mac 打开供体验。

尚未覆盖各品牌真机、16 KB 内存页设备或所有宿主应用；模拟器结果不代表全部机型兼容。翻译模型可能误译，双向切换有首次加载开销。

## 历史版本记录 / Previous versions

# Android 0.1.4 测试记录

日期：2026-10-21。设备：独立、全新数据的 Android 16 / API 36 arm64 模拟器。没有读取真实手机或 Mac 的剪贴板。

## 0.1.4 内置模型验收

- 清空专用 QA 模拟器的应用数据，先关闭 Wi-Fi 和移动网络（`dumpsys connectivity` 显示 `Active default network: none`），再覆盖安装完整 APK。无旧版语言包缓存，首次从 APK 内提取并校验模型，成功加载与实际翻译，全程没有恢复网络。
- 独立首次冷启动模型测试通过；再次完整回归 **14 项设备测试全部通过**：内置模型翻译、本地解压进度、剪贴板安全规则 5 项、键盘交互 5 项、设置页 2 项。10 项 JVM 测试通过；干净构建与 Android Lint 通过，0 错误 / 6 警告。
- 真正推理结果：`你好 → Hello.`、`我今天去北京。 → I went to Beijing today.`、`为什么不一起翻译了？ → Why don't you translate it together?`。本次模拟器用时分别约 153 / 273 / 402 ms，包含准备的模型测试合计约 1.66 秒；这些数字不代表真机速度或翻译质量保证。
- 实际软键盘验证整句上下文、中英文切换、默认英文上屏、长按复制 `Hello.`、密码保护、剪贴板插入/删除/清空、Shift/符号/表情/长按退格。24 小时过期和加密持久化规则维持通过。
- 检查最终 APK：包含约 110.4 MiB 的模型、词表和 tokenizer 文件以及模型归属、CC BY 4.0 / MIT 许可证、上游第三方声明；权限只有 `RECEIVE_BOOT_COMPLETED`，无 `INTERNET` 或 `ACCESS_NETWORK_STATE`。Google ML Kit 依赖已移除。复核 411 dp 默认字号设置页截图，模型显示“内置，无需下载”，隐私说明与实现一致。
- 已知兼容限制：本次为 Android 16 / API 36、arm64、**4 KB 内存页**模拟器。Extensions 0.13.0 的 arm64 tokenizer 预编译库尚未 16 KB ELF 对齐（3 条 Lint 警告），16 KB 页手机尚不承诺支持；其余 3 条是既有绘制/无障碍建议。需要后续重编译依赖并真机验证。未验证所有厂商、32 位手机或真实耗电，不用于直接上架 Google Play。
- 同一应用标识及开发签名，用户可覆盖安装以保留偏好与未过期历史；仅在无私人数据的 QA 模拟器清空过应用数据。

以下为历史版本的验证记录，旧 ML Kit 下载流程不再适用于 0.1.4。

## 0.1.3 设置页验收

- 4 项设备检查通过：设置偏好开关与清空确认、隐私展开和收起、实际界面截图、下载进度状态展示，以及嵌套卡片中的中文输入和历史插入。截图检查计入设置测试用例，未将历史版本的模型下载测试当作本次重跑。
- 在约 411 dp 宽度默认字号，以及约 360 dp 宽度 / 130% 系统字号下检查了页面排版；没有标题、开关或按钮重叠，页面可滚动查看。
- 生产设置页仍启用防截屏；测试仅在独立 QA Activity 上临时关闭。截图中的下载百分比来自实际进行中的模型下载。
- 构建和 Lint 通过。改版沿用同一应用标识及签名，可覆盖安装，原有偏好和未过期复制历史继续使用。

## 0.1.2 下载进度验收

- 10 项 JVM 测试通过，新增未知总大小不显示虚假百分比、实际字节换算和越界、暂停进度保留、下载完成与模型就绪区分。
- 3 项进度设备测试通过：系统字段/地址过滤、等待/50%/校验/完成 UI，以及已安装模型的 100% 成功状态。
- 在独立 QA 模拟器中删除测试模型后，使用真实 ML Kit 下载观察到 `1% · 0.4 MB / 33.7 MB`，页面关闭再重开后恢复当前进度，单独的真实下载及恢复测试通过。截图为实际下载，不是模拟数值。
- 构建和 Lint 通过。没有增加存储或访问其他应用下载的权限。真机厂商限制或 SDK 下载路径变化时会使用无百分比的等待状态。

## 0.1.1 界面与触摸验收

- 5 项键盘设备测试通过：实际坐标触摸按键边缘、长按退格连续删除、Shift 单字大写、符号/表情返回字母、中英切换、发送动作、整句英文及复制、密码保护、历史卡片插入及清空。
- 单独复核卡片右侧菜单，删除指定记录及顶部清空均通过。
- 按键布局检查确认第二排错位，Shift / 退格位于第三排两端；截图使用本次模拟器实际渲染结果。
- 编译与 Lint 通过。尚未在用户手机上验证真实触感，震动取决于系统设置和设备硬件。

## 首版沿用的验证

- Debug APK 编译、测试 APK 编译以及 Android Lint 均通过。Lint 无错误；保留原生 Switch、中文字符串等样式/国际化建议。
- 7 项 JVM 测试通过：真实词库的全拼、首字母、切分与相邻错拼；24 小时精确边界、去重不续期、无效内容拒绝和 100 条上限。
- 8 项设备测试通过：Keystore 加密文件不含测试明文；重新创建存储对象可读；到期记录不可插入；删除、清空、密文篡改和敏感标记过滤。
- 实际点击软键盘分三次输入“我 / 今天 / 去北京”，正文为“我今天去北京”，句子上下文保留完整。
- 切到英文后能继续输入 abc；打开剪贴板，点击一小时前的历史会插入到当前输入框；清空会删除全部历史。
- 使用真实本机模型验证“你好”显示英文，长按复制的内容与预览一致；开启默认英文后，空格直接提交英文。
- 密码输入框显示“安全输入”，剪贴板按钮禁用，仍可直接输入英文。
- 测试使用虚构文字；截图仅在测试用的 Activity 中临时关闭防截屏标志，生产设置页保留防截屏。

## 翻译和断网验收

首次 180 秒模型下载测试因网络缓慢超时；系统重试后，约 35 MB 的中英模型已完整下载，随后真实翻译测试通过。“你好，今天我去北京。”输出 **Hello, I am going to Beijing today.**

关闭模拟器 Wi-Fi 和移动网络，并确认系统报告 `Active default network: none` 后，使用已安装模型重新翻译，同样得到上述英文，测试通过。这个结果证明正文可离线翻译，不代表 SDK 在有网络时不会下载更新或发送指标。

## 仍需真机验收

尚未在实际安卓手机及不同厂商系统验证，也未覆盖所有第三方编辑器。首次模型下载受网络影响，Google 模型服务不可达时不能准备翻译；键盘和复制历史独立可用。试用 APK 使用开发签名，不是应用商店正式发行包。

## 截图

[空闲键盘布局](docs/layout.png) · [整句英文界面](docs/keyboard.png) · [复制历史面板](docs/clipboard.png)
