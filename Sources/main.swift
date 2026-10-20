import AppKit
import Combine
import SwiftUI
import InputMethodKit
import Carbon

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate, NSMenuDelegate {
    static let shared = AppDelegate()
    private var statusItem: NSStatusItem?
    private var window: NSWindow?
    private var modeSubscription: AnyCancellable?
    func applicationDidFinishLaunching(_ notification: Notification) {
        DistributedNotificationCenter.default().addObserver(self, selector: #selector(openSettings), name: Notification.Name("com.fuyi.inputmethod.FloatingTranslate.OpenSettings"), object: nil)
        DistributedNotificationCenter.default().addObserver(self, selector: #selector(inputSourceChanged(_:)), name: Notification.Name(kTISNotifySelectedKeyboardInputSourceChanged as String), object: nil)
        NSWorkspace.shared.notificationCenter.addObserver(self, selector: #selector(targetApplicationDeactivated(_:)), name: NSWorkspace.didDeactivateApplicationNotification, object: nil)
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        statusItem?.button?.title = "浮译"
        let menu = NSMenu()
        let settings = NSMenuItem(title: "浮译设置与试用…", action: #selector(openSettings), keyEquivalent: "")
        settings.target = self; menu.addItem(settings)
        let enable = NSMenuItem(title: "启用并试用浮译…", action: #selector(openInputSetup), keyEquivalent: "")
        enable.target = self; menu.addItem(enable)
        for (title, tag) in [("中文拼音", 0), ("English 直接输入", 1)] {
            let mode = NSMenuItem(title: title, action: #selector(selectInputMode(_:)), keyEquivalent: "")
            mode.target = self; mode.tag = tag; menu.addItem(mode)
        }
        menu.addItem(.separator())
        let privacy = NSMenuItem(title: "切换隐私模式（直接输入英文）", action: #selector(togglePrivacy), keyEquivalent: "")
        privacy.target = self; menu.addItem(privacy)
        let quit = NSMenuItem(title: "退出浮译", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "")
        menu.addItem(quit); menu.delegate = self; statusItem?.menu = menu
        modeSubscription = Preferences.shared.$directEnglish.sink { [weak self] english in
            self?.statusItem?.button?.title = english ? "浮译 · EN" : "浮译 · 中"
        }
        if CommandLine.arguments.contains("--snapshot") || CommandLine.arguments.contains("--settings") || !UserDefaults.standard.bool(forKey: "didShowWelcome") {
            showSettings()
            UserDefaults.standard.set(true, forKey: "didShowWelcome")
            if CommandLine.arguments.contains("--snapshot") {
                DispatchQueue.main.asyncAfter(deadline: .now() + 2) { [weak self] in
                    guard let view = self?.window?.contentView,
                          let bitmap = view.bitmapImageRepForCachingDisplay(in: view.bounds) else { NSApp.terminate(nil); return }
                    view.cacheDisplay(in: view.bounds, to: bitmap)
                    if let data = bitmap.representation(using: .png, properties: [:]) {
                        try? data.write(to: URL(fileURLWithPath: Bundle.main.bundleURL.deletingLastPathComponent().appendingPathComponent("preview.png").path))
                    }
                    NSApp.terminate(nil)
                }
            }
        }
    }
    @objc private func inputSourceChanged(_ notification: Notification) {
        DispatchQueue.main.async {
            let source = TISCopyCurrentKeyboardInputSource().takeRetainedValue()
            guard let pointer = TISGetInputSourceProperty(source, kTISPropertyInputSourceID) else { return }
            let identifier = Unmanaged<CFString>.fromOpaque(pointer).takeUnretainedValue() as String
            FloatingInputController.inputSourceDidChange(to: identifier)
        }
    }
    @objc private func targetApplicationDeactivated(_ notification: Notification) {
        FloatingInputController.applicationDidDeactivate(bundleIdentifier: (notification.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication)?.bundleIdentifier)
    }
    func menuWillOpen(_ menu: NSMenu) {
        for item in menu.items where item.action == #selector(selectInputMode(_:)) {
            item.state = (item.tag == 1) == Preferences.shared.directEnglish ? .on : .off
        }
    }
    @objc private func selectInputMode(_ item: NSMenuItem) { Preferences.shared.directEnglish = item.tag == 1 }
    @objc private func togglePrivacy() { Preferences.shared.privacyMode.toggle() }
    func windowWillClose(_ notification: Notification) {
        NotificationCenter.default.post(name: .clearPrivateText, object: nil)
    }
    func applicationWillTerminate(_ notification: Notification) {
        FloatingPanel.shared.hide()
        NotificationCenter.default.post(name: .clearPrivateText, object: nil)
    }
    @objc private func openSettings() { showSettings() }
    @objc func openInputSetup() {
        let installed = InstallationManager.shared.destination.appendingPathComponent("Contents/Helpers/FuyiInputSetup.app")
        guard FileManager.default.fileExists(atPath: installed.path) else {
            InstallationManager.shared.status = "请先点击“安装 / 修复输入法”，再启用浮译。"
            showSettings(); return
        }
        NSWorkspace.shared.open(installed)
    }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        showSettings(); return true
    }
    func showSettings() {
        if window == nil {
            let view = NSHostingController(rootView: SettingsView())
            let newWindow = NSWindow(contentViewController: view)
            newWindow.title = "浮译 · 输入与翻译"
            newWindow.styleMask = [.titled, .closable, .miniaturizable]
            newWindow.isReleasedWhenClosed = false
            newWindow.delegate = self
            newWindow.center()
            window = newWindow
        }
        window?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
}
// Establish IMK before creating NSApplication, as in the macOS Rime frontend.
// Resolve the Swift controller class explicitly before IMK reads its plist name.
func createInputServer() -> IMKServer? {
    guard !CommandLine.arguments.contains("--install") else { return nil }
    let controllerName = NSStringFromClass(FloatingInputController.self)
    precondition(Bundle.main.object(forInfoDictionaryKey: "InputMethodServerControllerClass") as? String == controllerName)
    guard let identifier = Bundle.main.bundleIdentifier,
          let connection = Bundle.main.object(forInfoDictionaryKey: "InputMethodConnectionName") as? String,
          connection == identifier + "_Connection" else {
        fatalError("Input service connection name must match the bundle identifier")
    }
    guard let server = IMKServer(name: connection, bundleIdentifier: identifier) else {
        fatalError("Could not establish the input service connection")
    }
    return server
}
// Opening a downloaded/build copy must not create a second IMK connection.
// Installer helpers run independently and never start an input server.
if !CommandLine.arguments.contains("--install"),
   let identifier = Bundle.main.bundleIdentifier,
   let existing = NSRunningApplication.runningApplications(withBundleIdentifier: identifier).first(where: { $0.processIdentifier != ProcessInfo.processInfo.processIdentifier && !$0.isTerminated }) {
    DistributedNotificationCenter.default().postNotificationName(Notification.Name(identifier + ".OpenSettings"), object: nil, userInfo: nil, deliverImmediately: true)
    existing.activate(options: [.activateAllWindows])
    exit(0)
}
let inputServer = createInputServer()
let app = NSApplication.shared
app.setActivationPolicy(.accessory)
if CommandLine.arguments.contains("--install") {
    let succeeded = MainActor.assumeIsolated { InstallationManager.shared.installOrRepair(openSettings: !CommandLine.arguments.contains("--background-install")) }
    MainActor.assumeIsolated { print(InstallationManager.shared.status) }
    exit(succeeded ? 0 : 1)
} else {
    MainActor.assumeIsolated { app.delegate = AppDelegate.shared }
    app.run()
}
