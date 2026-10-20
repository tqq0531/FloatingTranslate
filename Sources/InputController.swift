import AppKit
import InputMethodKit

/// IMK owns delivery; the independently testable session owns composition state.
@MainActor
@objc(FloatingInputController)
final class FloatingInputController: IMKInputController {
    private let session: InputSession
    override init!(server: IMKServer!, delegate: Any!, client: Any!) {
        session = InputSession(client: client as? IMKTextInput)
        super.init(server: server, delegate: delegate, client: client)
    }
    override func recognizedEvents(_ sender: Any!) -> Int { session.eventMask(sender) }
    override func handle(_ event: NSEvent!, client sender: Any!) -> Bool {
        session.process(event, client: (sender as? IMKTextInput) ?? client())
    }
    override func activateServer(_ sender: Any!) { session.begin((sender as? IMKTextInput) ?? client()) }
    override func deactivateServer(_ sender: Any!) { session.end(sender) }
    override func commitComposition(_ sender: Any!) { session.flush(sender) }
    override func hidePalettes() { session.conceal(); super.hidePalettes() }
    static func inputSourceDidChange(to identifier: String?) { InputSession.inputSourceDidChange(to: identifier) }
    static func applicationDidDeactivate(bundleIdentifier: String?) { InputSession.applicationDidDeactivate(bundleIdentifier: bundleIdentifier) }
    override func menu() -> NSMenu! {
        let menu = NSMenu(title: "浮译")
        let item = NSMenuItem(title: "浮译设置与试用…", action: #selector(showSettings(_:)), keyEquivalent: "")
        item.target = self; menu.addItem(item); return menu
    }
    @objc private func showSettings(_ sender: Any?) { AppDelegate.shared.showSettings() }
}
