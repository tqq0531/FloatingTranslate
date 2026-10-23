# Mac 0.5.12 中英混输整句翻译 / Mixed-language sentence translation

修复轻按 Shift 切换英文、输入大写字母后丢失前面中文的问题。当前由浮译输入的句子可以包含英文、数字与空格，再切回拼音继续整句翻译。例如：中文输入“因为这是”，Shift 切换英文输入 mac，再 Shift 切回输入“版本的”。

只有浮译当前会话内的句子参与累计，不读取文档前文。移动光标、选区、切换输入框、快捷键、隐私模式以及超时仍结束当前句子。空闲英文模式继续直接输入，不启动独立英文全文采集。应用提供可信位置时才允许整句替换，否则只提供翻译复制。

Fixed sentence loss after Shift language switching or Caps Lock inside a Chinese sentence. English letters, digits and spaces remain part of the active sentence. Example: compose 因为这是, switch to English for mac, then return to Pinyin for 版本的. Only text from the active IME session is accumulated; surrounding document text is never read. Caret/selection changes, shortcuts, focus changes, privacy and expiry still end the session. Idle English remains a passthrough.

已通过 InputSession 控制器回归：中文/英文/中文连续输入、Caps Lock、定时器及异步翻译返回、文档不读取、现有 Chrome/Qt 输入逻辑。实际第三方宿主仍由用户验收。

Controller regressions cover mixed scripts, Caps Lock, timer and asynchronous translation, no surrounding reads, and existing Chrome/Qt behavior. User validation in actual third-party hosts remains required.
