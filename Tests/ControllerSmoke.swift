import AppKit
import InputMethodKit

@MainActor final class AppDelegate {
    static let shared = AppDelegate()
    func showSettings() {}
}

final class TextClient: NSObject, IMKTextInput {
    var text = ""
    var marked = NSRange(location: NSNotFound, length: 0)
    var caret = 0
    var markedCalls = 0
    var inserts = 0
    var documentAccess = true
    var malformedQtRanges = false
    var reads = 0
    var onInsert: (() -> Void)?
    var overridden = ""
    var bundleID = "com.google.Chrome"
    var processingKey = false
    var rangeRequests = 0
    var geometryRequests = 0
    var geometryInsideKey = 0
    var invalidGeometry = false
    func plain(_ object: Any?) -> String { (object as? NSAttributedString)?.string ?? (object as? String ?? "") }
    func replace(_ value: String, range: NSRange) {
        text = (text as NSString).replacingCharacters(in: range, with: value)
        caret = range.location + value.utf16.count
    }
    func insertText(_ string: Any!, replacementRange: NSRange) {
        inserts += 1
        let range = replacementRange.location != NSNotFound ? replacementRange : (marked.location != NSNotFound ? marked : NSRange(location: caret, length: 0))
        replace(plain(string), range: range); marked = NSRange(location: NSNotFound, length: 0)
        onInsert?()
    }
    func setMarkedText(_ string: Any!, selectionRange: NSRange, replacementRange: NSRange) {
        markedCalls += 1
        let value = plain(string)
        let range = marked.location != NSNotFound ? marked : NSRange(location: caret, length: 0)
        replace(value, range: range)
        marked = value.isEmpty ? NSRange(location: NSNotFound, length: 0) : NSRange(location: range.location, length: value.utf16.count)
    }
    func selectedRange() -> NSRange { rangeRequests += 1; if malformedQtRanges { return NSRange(location: -text.utf8.count, length: text.utf8.count) }; return documentAccess ? NSRange(location: caret, length: 0) : NSRange(location: NSNotFound, length: NSNotFound) }
    func markedRange() -> NSRange { rangeRequests += 1; if malformedQtRanges && marked.location != NSNotFound { return NSRange(location: -(text as NSString).substring(to: marked.location).utf8.count, length: marked.length) }; return documentAccess ? marked : NSRange(location: NSNotFound, length: NSNotFound) }
    func attributedSubstring(from range: NSRange) -> NSAttributedString! { reads += 1; return nil }
    func length() -> Int { NSNotFound }
    func characterIndex(for point: NSPoint, tracking mappingMode: IMKLocationToOffsetMappingMode, inMarkedRange: UnsafeMutablePointer<ObjCBool>!) -> Int { NSNotFound }
    func attributes(forCharacterIndex index: Int, lineHeightRectangle: UnsafeMutablePointer<NSRect>!) -> [AnyHashable: Any]! {
        geometryRequests += 1
        if processingKey { geometryInsideKey += 1 }
        if invalidGeometry { lineHeightRectangle?.pointee = .zero; return [:] }
        lineHeightRectangle?.pointee = NSRect(x: 400, y: 400, width: 1, height: 22); return [:]
    }
    func validAttributesForMarkedText() -> [Any]! { [] }
    func overrideKeyboard(withKeyboardNamed keyboardUniqueName: String!) { overridden = keyboardUniqueName }
    func selectMode(_ modeIdentifier: String!) {}
    func supportsUnicode() -> Bool { true }
    func bundleIdentifier() -> String! { bundleID }
    func windowLevel() -> CGWindowLevel { 0 }
    func supportsProperty(_ property: TSMDocumentPropertyTag) -> Bool { false }
    func uniqueClientIdentifierString() -> String! { "fixture-client" }
    func string(from range: NSRange, actualRange: NSRangePointer!) -> String! { reads += 1; return nil }
    func firstRect(forCharacterRange range: NSRange, actualRange: NSRangePointer!) -> NSRect { NSRect(x: 400, y: 400, width: 1, height: 22) }
}

@main struct ControllerSmoke {
    @MainActor static func main() async throws {
        _ = NSApplication.shared
        let savedEnglish = Preferences.shared.directEnglish
        let savedDefault = Preferences.shared.englishDefault
        let savedPrivacy = Preferences.shared.privacyMode
        Preferences.shared.directEnglish = false; Preferences.shared.englishDefault = false; Preferences.shared.privacyMode = false
        defer { Preferences.shared.directEnglish = savedEnglish; Preferences.shared.englishDefault = savedDefault; Preferences.shared.privacyMode = savedPrivacy }
        let client = TextClient()
        let controller = InputSession(client: client)
        func check(_ condition: Bool, _ message: String) { precondition(condition, message); print("PASS \(message)") }
        func key(_ text: String, code: UInt16 = 0, modifiers: NSEvent.ModifierFlags = []) -> Bool {
            let event = NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: modifiers, timestamp: ProcessInfo.processInfo.systemUptime, windowNumber: 0, context: nil, characters: text, charactersIgnoringModifiers: text, isARepeat: false, keyCode: code)!
            client.processingKey = true
            defer { client.processingKey = false }
            return controller.process(event, client: client)
        }
        func type(_ word: String) { for char in word { check(key(String(char)), "composition receives \(char)") }; _ = key(" ", code: 49) }
        controller.begin(client)
        if CommandLine.arguments.contains("--missing-engine") {
            check(!key("a") && client.markedCalls == 0, "unavailable engine passes typing to the app instead of swallowing it")
            check(!key("", code: 53), "idle Escape reaches the app")
            controller.end(client)
            return
        }
        check(client.overridden == "com.apple.keylayout.ABC", "activation restores a known keyboard layout")
        client.onInsert = { [weak controller, weak client] in controller?.flush(client) }
        type("wo"); type("jintian"); type("qubeijing")
        client.onInsert = nil
        check(client.text == "我今天去北京", "controller commits separate chunks without dropping or duplicating text")
        check(FloatingPanel.shared.model.source == "我今天去北京", "controller translates the whole accumulated sentence")
        try await Task.sleep(for: .seconds(1))
        check(!FloatingPanel.shared.model.english.isEmpty, "controller receives a real whole-sentence translation")
        let selectedTranslation = FloatingPanel.shared.renderedEnglish
        FloatingPanel.shared.translationText.setSelectedRange(NSRange(location: 0, length: 2))
        controller.end(client)
        check(FloatingPanel.shared.renderedEnglish == selectedTranslation && FloatingPanel.shared.isSelectingTranslation, "IME deactivation during translation selection preserves the copyable English snapshot")
        FloatingPanel.shared.finishTextSelection(); FloatingPanel.shared.hide()
        controller.begin(client)
        check(client.rangeRequests == 0 && client.geometryInsideKey == 0, "Chrome receives no synchronous document-range or geometry query during key processing")
        check(client.geometryRequests <= 1, "Chrome caret geometry is reused instead of polled every 100 ms")
        check(!key("l", code: 37, modifiers: .command), "Command L passes through to the browser")
        check(FloatingPanel.shared.model.source.isEmpty, "browser shortcut clears sentence context")
        _ = key("n"); _ = key("h")
        let ownModeText = client.text, ownModeMarked = client.marked
        InputSession.inputSourceDidChange(to: "com.fuyi.inputmethod.FloatingTranslate.Hans")
        check(client.text == ownModeText && client.marked == ownModeMarked && client.marked.location != NSNotFound, "own Hans mode notification preserves active composition")
        InputSession.inputSourceDidChange(to: "com.apple.keylayout.ABC")
        check(client.text == "我今天去北京你好" && client.marked.location == NSNotFound, "programmatic source switch finalizes marked text")
        let inserts = client.inserts
        controller.end(client)
        check(client.inserts == inserts, "duplicate deactivation cannot commit text twice")
        controller.begin(client); client.documentAccess = false
        type("nhm")
        check(client.text.hasSuffix("你好吗"), "client without document ranges can still type Chinese")
        controller.flush(client)
        check(FloatingPanel.shared.model.source.isEmpty, "host composition end clears range-free Chrome sentence context")
        _ = key("n")
        InputSession.applicationDidDeactivate(bundleIdentifier: "com.example.unrelated")
        check(client.marked.location != NSNotFound, "unrelated app deactivation does not interrupt current typing")
        InputSession.applicationDidDeactivate(bundleIdentifier: "com.google.Chrome")
        check(client.marked.location == NSNotFound && FloatingPanel.shared.model.source.isEmpty, "target app deactivation clears composition and floating text")
        check(client.reads == 0, "controller never reads surrounding document text")
        controller.begin(client)
        Preferences.shared.directEnglish = true
        let calls = client.markedCalls
        check(!key("a") && client.markedCalls == calls, "English mode passes characters directly without marked text")
        Preferences.shared.directEnglish = false
        check(!key("", code: 53), "idle Escape reaches browser controls")
        controller.end(client)
        controller.begin(client)
        _ = key("n")
        let otherClient = TextClient()
        let other = InputSession(client: otherClient)
        other.begin(otherClient)
        check(client.marked.location == NSNotFound, "activating another client ends the previous marked composition")
        let otherEvent = NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime, windowNumber: 0, context: nil, characters: "n", charactersIgnoringModifiers: "n", isARepeat: false, keyCode: 0)!
        check(other.process(otherEvent, client: otherClient), "new client begins an independent composition")
        controller.end(client)
        check(FloatingPanel.shared.model.pinyin == "n", "late deactivation of the old client cannot clear the new client")
        other.end(otherClient)
        let qt = TextClient(); qt.bundleID = "www.itdp.cn"; qt.malformedQtRanges = true
        let qtSession = InputSession(client: qt); qtSession.begin(qt)
        func qtKey(_ text: String, code: UInt16 = 0) -> Bool {
            let e = NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime, windowNumber: 0, context: nil, characters: text, charactersIgnoringModifiers: text, isARepeat: false, keyCode: code)!
            return qtSession.process(e, client: qt)
        }
        for chunk in ["wo", "jintian", "qubeijing"] {
            for c in chunk { _ = qtKey(String(c)) }; _ = qtKey(" ", code: 49)
            try await Task.sleep(for: .milliseconds(200))
        }
        check(qt.text == "我今天去北京" && FloatingPanel.shared.model.source == qt.text, "negative Qt ranges preserve separately committed sentence across timer ticks")
        try await Task.sleep(for: .seconds(1))
        check(!FloatingPanel.shared.model.english.isEmpty, "Qt editor receives real whole-sentence translation")
        _ = qtKey("n"); _ = qtKey("h")
        try await Task.sleep(for: .seconds(1))
        check(FloatingPanel.shared.model.source == "我今天去北京你好", "Qt preview includes committed prefix and current candidate")
        let beforeUnsafeReplace = qt.text
        _ = qtKey("\t", code: 48)
        check(qt.text == beforeUnsafeReplace && FloatingPanel.shared.model.status.contains("无法安全替换"), "invalid Qt document ranges cannot replace previously committed text")
        _ = qtKey(" ", code: 49)
        _ = qtKey("", code: 123)
        check(FloatingPanel.shared.model.source.isEmpty, "navigation clears range-free Qt sentence context")
        qtSession.end(qt)
        check(qt.reads == 0, "Qt workaround never reads surrounding editor text")
        let missed = TextClient(); missed.invalidGeometry = true
        let missingSession = InputSession(client: missed); missingSession.begin(missed)
        _ = missingSession.process(otherEvent, client: missed)
        try await Task.sleep(for: .milliseconds(550))
        check(missed.geometryRequests <= 1 && missed.rangeRequests == 0, "invalid Chrome geometry is throttled without fallback range queries")
        missingSession.end(missed)
        let beforeStale = missed.geometryRequests
        try await Task.sleep(for: .milliseconds(150))
        check(missed.geometryRequests == beforeStale && FloatingPanel.shared.model.source.isEmpty, "ended session cannot perform queued browser geometry work")
        print("PASS controller fixture complete; real Chrome still requires manual verification")
    }
}
