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
    var markedPayload: Any?
    var markedSelection = NSRange(location: NSNotFound, length: 0)
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
        markedPayload = string
        markedSelection = selectionRange
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
        let savedRemember = Preferences.shared.rememberCandidates
        let savedChinese = Preferences.shared.chineseDefault
        Preferences.shared.rememberCandidates = false
        let savedEnglish = Preferences.shared.directEnglish
        let savedDefault = Preferences.shared.englishDefault
        let savedPrivacy = Preferences.shared.privacyMode
        Preferences.shared.directEnglish = false; Preferences.shared.englishDefault = false; Preferences.shared.privacyMode = false
        defer { Preferences.shared.rememberCandidates = savedRemember; Preferences.shared.chineseDefault = savedChinese; Preferences.shared.directEnglish = savedEnglish; Preferences.shared.englishDefault = savedDefault; Preferences.shared.privacyMode = savedPrivacy }
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
        for c in "nihaoma" { _ = key(String(c)) }
        let undoCandidates = FloatingPanel.shared.model.candidates
        let partialIndex = undoCandidates.firstIndex { $0.code.count < "nihaoma".count }!
        FloatingPanel.shared.onChoose?(partialIndex)
        check(client.text != "nihaoma" && client.marked.location != NSNotFound, "partial selection stays composed")
        _ = key("", code: 51)
        check(client.text == "nihaoma", "Backspace restores the selected segment and preserves remaining Pinyin")
        check(!FloatingPanel.shared.model.candidates.isEmpty, "restored Pinyin has selectable candidates")
        _ = key("", code: 53)
        client.text = ""; client.caret = 0
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
        check(key("a") && client.markedCalls > calls, "English mode composes a whole sentence for Chinese translation")
        let englishMark = client.markedPayload as? NSAttributedString
        check(englishMark?.attribute(.underlineStyle, at: 0, effectiveRange: nil) as? Int == 0,
              "English composition explicitly disables the host underline")
        check((englishMark?.attribute(.backgroundColor, at: 0, effectiveRange: nil) as? NSColor)?.alphaComponent == 0,
              "English composition has a transparent background")
        check(client.markedSelection == NSRange(location: 1, length: 0), "English typing leaves a caret instead of selecting the sentence")
        let nativeEditor = NSTextView(frame: NSRect(x: 0, y: 0, width: 600, height: 100))
        nativeEditor.setMarkedText(englishMark!, selectedRange: client.markedSelection, replacementRange: NSRange(location: NSNotFound, length: 0))
        var nativeAttributes = nativeEditor.textStorage!.attributes(at: 0, effectiveRange: nil)
        for (key, value) in nativeEditor.layoutManager!.temporaryAttributes(atCharacterIndex: 0, effectiveRange: nil) { nativeAttributes[key] = value }
        check((nativeAttributes[.underlineStyle] as? Int ?? 0) == 0 && (nativeAttributes[.backgroundColor] as? NSColor)?.alphaComponent ?? 0 == 0,
              "native AppKit editor has no background or underline on English composition")
        check(nativeEditor.selectedRange().length == 0, "native AppKit editor does not select the English sentence")
        _ = key("", code: 53)
        Preferences.shared.directEnglish = false
        check(!key("", code: 53), "idle Escape reaches browser controls")
        controller.end(client)
        controller.begin(client)
        _ = key("n")
        check(client.markedPayload is String, "Pinyin keeps the standard host composition appearance")
        let otherClient = TextClient()
        let other = InputSession(client: otherClient)
        other.begin(otherClient)
        check(client.marked.location == NSNotFound, "activating another client ends the previous marked composition")
        let otherEvent = NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime, windowNumber: 0, context: nil, characters: "n", charactersIgnoringModifiers: "n", isARepeat: false, keyCode: 0)!
        check(other.process(otherEvent, client: otherClient), "new client begins an independent composition")
        controller.end(client)
        check(FloatingPanel.shared.model.pinyin == "n", "late deactivation of the old client cannot clear the new client")
        other.end(otherClient)
        let englishClient = TextClient(); englishClient.bundleID = "com.example.Editor"
        let englishSession = InputSession(client: englishClient)
        func englishKey(_ text: String, code: UInt16 = 0) -> Bool {
            let event = NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: [], timestamp: ProcessInfo.processInfo.systemUptime, windowNumber: 0, context: nil, characters: text, charactersIgnoringModifiers: text, isARepeat: false, keyCode: code)!
            return englishSession.process(event, client: englishClient)
        }
        for chineseDefault in [false, true] {
            for alternative in [false, true] {
                Preferences.shared.directEnglish = true
                Preferences.shared.chineseDefault = chineseDefault
                Preferences.shared.englishDefault = !chineseDefault
                englishClient.text = ""; englishClient.caret = 0
                englishSession.begin(englishClient)
                for c in "Hello world." { check(englishKey(String(c), code: c == " " ? 49 : 0), "English sentence accepts characters and word spaces") }
                check(FloatingPanel.shared.model.source == "Hello world." && englishClient.marked.location != NSNotFound, "word spaces keep English sentence composed")
                for _ in 0..<40 {
                    if !FloatingPanel.shared.model.english.isEmpty { break }
                    try await Task.sleep(for: .milliseconds(100))
                }
                let target = FloatingPanel.shared.model.english
                check(!target.isEmpty && target.contains(where: { !$0.isASCII }) && FloatingPanel.shared.model.toChinese, "English sentence receives actual Chinese translation and direction badge")
                check(englishKey("", code: alternative ? 48 : 36), "Enter or Tab submits English mode")
                check(englishClient.text == ((chineseDefault != alternative) ? target : "Hello world."), "independent English default and alternative output choose correct language")
                check(englishClient.marked.location == NSNotFound, "English submit ends marked composition")
                englishSession.end(englishClient)
            }
        }
        func englishShift() {
            let time = ProcessInfo.processInfo.systemUptime
            for (flags, stamp) in [(NSEvent.ModifierFlags.shift, time), (NSEvent.ModifierFlags(), time + 0.05)] {
                let event = NSEvent.keyEvent(with: .flagsChanged, location: .zero, modifierFlags: flags, timestamp: stamp, windowNumber: 0, context: nil, characters: "", charactersIgnoringModifiers: "", isARepeat: false, keyCode: 56)!
                _ = englishSession.process(event, client: englishClient)
            }
        }
        for englishDefault in [false, true] {
            Preferences.shared.directEnglish = false; Preferences.shared.englishDefault = englishDefault
            englishClient.text = ""; englishClient.caret = 0; englishSession.begin(englishClient)
            for c in "make" { _ = englishKey(String(c)) }
            englishShift()
            check(englishClient.text == "make" && englishClient.marked.location == NSNotFound, "Shift commits literal make instead of accepting Chinese candidate, independent of output default")
            check(Preferences.shared.directEnglish && FloatingPanel.shared.model.source == "make", "Shift preserves literal word in translation context and enters English mode")
            englishSession.end(englishClient)
        }
        Preferences.shared.directEnglish = false; Preferences.shared.englishDefault = false
        englishClient.text = ""; englishClient.caret = 0; englishSession.begin(englishClient)
        for c in "yinwei" { _ = englishKey(String(c)) }; _ = englishKey(" ", code: 49)
        for c in "make" { _ = englishKey(String(c)) }
        englishShift()
        check(englishClient.text == "因为make" && FloatingPanel.shared.model.source == "因为make", "Shift keeps earlier Chinese and raw English in the same sentence")
        englishShift()
        for c in "banben" { _ = englishKey(String(c)) }; _ = englishKey(" ", code: 49)
        check(englishClient.text == "因为make版本" && FloatingPanel.shared.model.source == englishClient.text, "Pinyin resumes after literal Shift without losing mixed sentence")
        englishSession.end(englishClient)
        Preferences.shared.directEnglish = false; Preferences.shared.englishDefault = false
        englishClient.text = ""; englishClient.caret = 0; englishSession.begin(englishClient)
        for c in "shi" { _ = englishKey(String(c)) }
        let firstPage = FloatingPanel.shared.model.candidates
        check(firstPage.count > 5, "Rime exposes candidates past first page")
        check(englishKey("=") && FloatingPanel.shared.model.selected == 5, "equals moves to next five-item page")
        check(englishKey("-") && FloatingPanel.shared.model.selected == 0, "minus returns to previous page")
        englishSession.end(englishClient)
        let mixed = TextClient(); mixed.bundleID = "com.example.Editor"
        let mixedSession = InputSession(client: mixed); mixedSession.begin(mixed)
        func mixedKey(_ text: String, code: UInt16 = 0, modifiers: NSEvent.ModifierFlags = []) -> Bool {
            let event = NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: modifiers, timestamp: ProcessInfo.processInfo.systemUptime, windowNumber: 0, context: nil, characters: text, charactersIgnoringModifiers: text, isARepeat: false, keyCode: code)!
            return mixedSession.process(event, client: mixed)
        }
        func mixedType(_ text: String) { for c in text { _ = mixedKey(String(c)) }; _ = mixedKey(" ", code: 49) }
        func mixedShift() {
            let time = ProcessInfo.processInfo.systemUptime
            for (flags, stamp) in [(NSEvent.ModifierFlags.shift, time), (NSEvent.ModifierFlags(), time + 0.05)] {
                let event = NSEvent.keyEvent(with: .flagsChanged, location: .zero, modifierFlags: flags, timestamp: stamp, windowNumber: 0, context: nil, characters: "", charactersIgnoringModifiers: "", isARepeat: false, keyCode: 56)!
                _ = mixedSession.process(event, client: mixed)
            }
        }
        mixedType("yinwei"); mixedType("zheshi")
        mixedShift()
        check(Preferences.shared.directEnglish, "Shift enters English without discarding the current sentence")
        for c in "mac" { check(mixedKey(String(c)), "literal English inside Chinese stays in tracked sentence") }
        try await Task.sleep(for: .milliseconds(350))
        check(FloatingPanel.shared.model.source == "因为这是mac", "timer and English translation callback preserve mixed context")
        mixedShift()
        check(!Preferences.shared.directEnglish, "Shift returns to Pinyin without discarding mixed context")
        mixedType("banben"); mixedType("de")
        check(mixed.text == "因为这是mac版本的" && FloatingPanel.shared.model.source == mixed.text, "mixed-language whole sentence survives returning to Pinyin")
        check(mixedKey("A", modifiers: .capsLock), "Caps Lock literal stays tracked")
        mixedType("ceshi")
        check(FloatingPanel.shared.model.source == mixed.text && mixed.text.contains("mac版本的A"), "uppercase English does not discard earlier Chinese")
        check(mixed.reads == 0, "mixed input never scans document content")
        mixedSession.end(mixed)
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
