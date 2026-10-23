# Mac 0.5.13 使用说明与验证 / Usage and verification

1. 安装后选择浮译，轻按 Shift 切换“中文拼音 / English 英译中”。
2. 设置中分别选择拼音默认英文与英文默认中文。英文模式中输入 `Hello world.`，空格用于分词；停顿后出现中文，回车提交默认语言，Tab 提交另一语言。Esc 保留英文原文。译文尚未就绪时请求译文的确认键不会提交旧译文。
3. `=` / `-` 翻页，每页五项；数字 1—5 或鼠标选择当前页。常选短词按相同拼音计数，完整长词/句优先，最多四字、2048 项。
4. “记住常选短词”可关闭；“清除选词记忆”删除本机钥匙串中的记录，钥匙串锁定时提示解锁重试。记录可能含私人短语；英文连续正文和译文不记录。
5. 浮窗支持中文或英文译文复制。中文夹英文仍按当前整句翻译；移动光标、快捷键、切换输入框、隐私模式或超时清理上下文。
6. 选择所需输入方向后点“下载 / 检查”，检查该方向的 Apple 语言包；状态分别显示两个方向是否准备好。已安装语言包时正文在本机翻译，未安装时不切换云翻译。

Shift switches direction. Each direction has an independent default output. In English mode, spaces extend the sentence, Enter commits its default, Tab the alternative, and Esc preserves the source. Candidate pages use `=` / `-`, five entries per page, with optional Keychain short-word ranking. Clear/disable controls are in Settings. Translation/prose history is not saved. Apple system models remain in use, with initial language preparation as needed. See PRIVACY.md for details and password-input limits.

开发验证：控制器检查英文单词空格、独立默认开关、Enter/Tab 输出、中文译文标记、翻页、中英混输、Chrome/Qt 回归及不读取文档前文；引擎检查短词排序/清除/不同拼音隔离/长句保护；真实 Apple 双向翻译、隐私源审计、应用构建与签名检查。模拟客户端不能代替每个第三方应用的实机验收。

Controller checks cover English spaces, independent defaults, Enter/Tab output, direction badge, paging, mixed context, Chrome/Qt regressions and no surrounding reads. Engine checks cover ranking, clearing, spelling isolation and long-phrase protection. Shared behavior is tested alongside actual Apple translation and build/signing checks. Fixtures do not replace all third-party host validation.

2026-10-23 本机实际双向结果：`我今天去北京。` → `I'm going to Beijing today.`；`Have you worked out today?` → `你今天锻炼了吗？`。控制器四种默认/替代输出组合通过；短词排序使用内存隔离测试。钥匙串在用户不同权限、锁定状态下的持久化仍需用户验证；访问失败不阻断输入，也不明文落盘。
