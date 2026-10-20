import AppKit
import Carbon

/// Manual integration check: real keyboard input is required by macOS IMK.
/// This app never synthesizes global key events or reads other applications.
@MainActor
final class ManualDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
    let window = NSWindow(contentRect: NSRect(x: 180, y: 200, width: 760, height: 420), styleMask: [.titled, .closable, .resizable], backing: .buffered, defer: false)
    let text = NSTextView(frame: NSRect(x: 20, y: 65, width: 720, height: 210))
    var original: TISInputSource?
    func applicationDidFinishLaunching(_ notification: Notification) {
        original = TISCopyCurrentKeyboardInputSource().takeRetainedValue()
        window.title = "浮译 · 手动输入验收（仅使用虚构文本）"
        window.delegate = self
        let help = NSTextField(wrappingLabelWithString: "请点击下方按钮选择浮译，再手动输入：\n① nh / nihoa + 空格应得到“你好”  ② nhm 应有“你好吗”\n③ 分别输入 wo、jintian、qubeijing 并确认，英文应按“我今天去北京”更新。\n④ 轻按 Shift 切换英文；移动窗口时浮窗应跟随。\n不要输入真实密码、密钥或聊天记录。")
        help.frame = NSRect(x: 20, y: 285, width: 720, height: 115)
        help.autoresizingMask = [.width, .minYMargin]
        text.font = .systemFont(ofSize: 20)
        text.autoresizingMask = [.width, .height]
        let choose = NSButton(title: "选择浮译并开始输入", target: self, action: #selector(selectFuyi))
        choose.frame = NSRect(x: 20, y: 20, width: 200, height: 32)
        let clear = NSButton(title: "清空测试内容", target: self, action: #selector(clearText))
        clear.frame = NSRect(x: 240, y: 20, width: 150, height: 32)
        for view in [help, text, choose, clear] { window.contentView?.addSubview(view) }
        window.makeKeyAndOrderFront(nil); window.makeFirstResponder(text)
        NSApp.activate(ignoringOtherApps: true)
    }
    @objc func selectFuyi() {
        window.makeFirstResponder(text)
        text.inputContext?.selectedKeyboardInputSource = "com.fuyi.inputmethod.FloatingTranslate.Hans"
    }
    @objc func clearText() { text.string = ""; window.makeFirstResponder(text) }
    func windowWillClose(_ notification: Notification) {
        text.string = ""
        if let original { _ = TISSelectInputSource(original) }
        NSApp.terminate(nil)
    }
}
let app = NSApplication.shared
app.setActivationPolicy(.regular)
let delegate = MainActor.assumeIsolated { ManualDelegate() }
app.delegate = delegate
app.run()
