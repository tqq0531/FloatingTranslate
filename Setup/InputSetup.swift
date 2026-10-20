import AppKit

/// A separate, ordinary text client activates IMK through its own input context.
/// No global key hooks, accessibility access, network requests or saved text.
@MainActor
final class InputSetupDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
    private let sourceID = "com.fuyi.inputmethod.FloatingTranslate.Hans"
    private let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 520, height: 320), styleMask: [.titled, .closable], backing: .buffered, defer: false)
    private let text = NSTextView(frame: NSRect(x: 28, y: 76, width: 464, height: 110))
    private let status = NSTextField(wrappingLabelWithString: "正在启用浮译…")
    func applicationDidFinishLaunching(_ notification: Notification) {
        window.title = "启用浮译"
        window.delegate = self; window.isReleasedWhenClosed = false
        let title = NSTextField(labelWithString: "开始使用浮译")
        title.font = .systemFont(ofSize: 24, weight: .semibold)
        title.frame = NSRect(x: 28, y: 260, width: 464, height: 34)
        let help = NSTextField(wrappingLabelWithString: "在下方输入 nihao，再按空格，应该出现“你好”和英文翻译。之后可从菜单栏选择浮译，在其他软件继续输入。")
        help.frame = NSRect(x: 28, y: 202, width: 464, height: 48)
        help.font = .systemFont(ofSize: 13)
        text.font = .systemFont(ofSize: 20)
        text.isRichText = false; text.drawsBackground = true
        text.textContainerInset = NSSize(width: 10, height: 10)
        text.isAutomaticSpellingCorrectionEnabled = false
        text.isAutomaticTextReplacementEnabled = false
        status.frame = NSRect(x: 28, y: 22, width: 340, height: 40)
        status.font = .systemFont(ofSize: 12)
        status.textColor = .secondaryLabelColor
        let retry = NSButton(title: "重新启用", target: self, action: #selector(activateInput))
        retry.frame = NSRect(x: 386, y: 28, width: 106, height: 32)
        for view in [title, help, text, status, retry] { window.contentView?.addSubview(view) }
        window.center(); window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.25) { [weak self] in self?.activateInput() }
    }
    @objc private func activateInput() {
        window.makeKeyAndOrderFront(nil); window.makeFirstResponder(text)
        guard let context = text.inputContext, context.keyboardInputSources?.contains(sourceID) == true else {
            status.stringValue = "系统尚未识别浮译。请先在浮译设置中点击“安装 / 修复输入法”。"
            return
        }
        context.activate()
        context.selectedKeyboardInputSource = sourceID
        status.stringValue = context.selectedKeyboardInputSource == sourceID
            ? "已在试用框选中浮译。输入内容只保留在内存，关闭窗口即清除。"
            : "尚未切换成功，请点击“重新启用”再试一次。"
    }
    func windowWillClose(_ notification: Notification) { text.string = ""; NSApp.terminate(nil) }
    func applicationWillTerminate(_ notification: Notification) { text.string = "" }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        window.makeKeyAndOrderFront(nil); activateInput(); return true
    }
}
let app = NSApplication.shared
app.setActivationPolicy(.regular)
let delegate = MainActor.assumeIsolated { InputSetupDelegate() }
app.delegate = delegate
app.run()
