import AppKit
import InputMethodKit

@MainActor
final class InputSession: NSObject {
    private static weak var activeController: InputSession?
    private weak var activeClient: IMKTextInput?
    private var clientUpdateDepth = 0
    private var markedText = ""
    private var clientBundleIdentifier = ""
    private var positionGeneration = 0
    private var needsFreshAnchor = true
    private var lastGeometryAttempt: TimeInterval = -.infinity
    private var isChromium: Bool { clientBundleIdentifier.hasPrefix("com.google.Chrome") || clientBundleIdentifier.hasPrefix("org.chromium.Chromium") }
    // Chrome range/rectangle requests can synchronously wait for a busy renderer.
    private func selectedRange(_ client: IMKTextInput) -> NSRange { isChromium ? replacement : validatedRange(client.selectedRange()) }
    private func markedRange(_ client: IMKTextInput) -> NSRange { isChromium ? replacement : validatedRange(client.markedRange()) }
    private func validatedRange(_ range: NSRange) -> NSRange {
        // Some Qt editors report negative positions and a nonzero selection after
        // committing Chinese. These are unavailable ranges, not user selections.
        guard range.location >= 0, range.location != NSNotFound,
              range.length >= 0, range.length != NSNotFound,
              range.location <= Int.max - range.length else {
            sentence.discardDocumentPosition()
            return replacement
        }
        return range
    }
    static let engine = PinyinEngine(url: Bundle.main.url(forResource: "pinyin_simp.dict", withExtension: "yaml"))
    private var raw = ""
    private var locked = ""
    private var selection = 0
    private var sentence = SentenceBuffer()
    private var translation = ""
    private var translatedSource = ""
    private var requestedSource = ""
    private var translationStatus = ""
    private var revision = 0
    private var work: Task<Void, Never>?
    private var trackingTimer: Timer?
    private var lastCaret = NSRect.zero
    private var shiftTap = ShiftTapDetector()
    private var modeToast: Task<Void, Never>?
    private let replacement = NSRange(location: NSNotFound, length: NSNotFound)
    private var now: TimeInterval { ProcessInfo.processInfo.systemUptime }
    private var choices: [PinyinCandidate] { Self.engine.candidates(raw) }
    private var inline: String { locked + raw }
    private var fragment: String {
        guard !raw.isEmpty else { return locked }
        let items = choices
        guard items.indices.contains(selection) else { return locked + raw }
        let chosen = items[selection]
        return locked + chosen.text + Self.engine.preview(String(raw.dropFirst(chosen.code.count)))
    }
    private var completeFragment: Bool {
        if raw.isEmpty { return !locked.isEmpty }
        guard choices.indices.contains(selection) else { return false }
        return Self.engine.isComplete(String(raw.dropFirst(choices[selection].code.count)))
    }
    private var source: String { sentence.text + (completeFragment ? fragment : "") }
    private func location(_ range: NSRange) -> Int? { range.location == NSNotFound ? nil : range.location }
    init(client: IMKTextInput?) {
        activeClient = client
        super.init()
    }
    func begin(_ sender: Any!, configureKeyboard: Bool = true) {
        guard let client = (sender as? IMKTextInput) ?? activeClient else { return }
        if let previous = Self.activeController, previous !== self { previous.end(nil, allowSelection: false) }
        if Self.activeController === self, let old = activeClient, old !== client { finishSession(old) }
        activeClient = client; Self.activeController = self
        clientBundleIdentifier = client.bundleIdentifier() ?? ""
        shiftTap.reset()
        if configureKeyboard { client.overrideKeyboard(withKeyboardNamed: "com.apple.keylayout.ABC") }
    }
    /// macOS 26 can omit deactivation after an out-of-process source switch.
    static func inputSourceDidChange(to identifier: String?) {
        guard let identifier, !["com.fuyi.inputmethod.FloatingTranslate", "com.fuyi.inputmethod.FloatingTranslate.Hans"].contains(identifier) else { return }
        activeController?.end(nil, allowSelection: false)
    }
    static func applicationDidDeactivate(bundleIdentifier: String?) {
        guard let identifier = bundleIdentifier, let controller = activeController,
              controller.clientBundleIdentifier == identifier else { return }
        controller.end(nil, allowSelection: false)
    }
    private var isActive: Bool { Self.activeController === self && activeClient != nil }
    func eventMask(_ sender: Any!) -> Int {
        Int(NSEvent.EventTypeMask.keyDown.rawValue | NSEvent.EventTypeMask.flagsChanged.rawValue)
    }
    func process(_ event: NSEvent!, client sender: Any!) -> Bool {
        guard let event, let client = (sender as? IMKTextInput) ?? activeClient else { return false }
        guard !PrivacyProtection.isBlocking else { shiftTap.reset(); reset(); return false }
        if Self.activeController !== self { begin(client, configureKeyboard: false) }
        activeClient = client
        FloatingPanel.shared.finishTextSelection()
        if event.type == .flagsChanged {
            let toggle = shiftTap.modifierChanged(keyCode: Int(event.keyCode), shiftDown: event.modifierFlags.contains(.shift), otherModifiers: !event.modifierFlags.intersection([.command, .control, .option]).isEmpty, time: event.timestamp)
            if toggle {
                commitLiteral(client); Preferences.shared.directEnglish.toggle(); showModeToast(client); return true
            }
            return false
        }
        guard event.type == .keyDown else { return false }
        shiftTap.noteKeyDown()
        if Preferences.shared.directEnglish || event.modifierFlags.contains(.capsLock) {
            if !inline.isEmpty || !sentence.isEmpty { commitLiteral(client) }
            return false
        }
        if !event.modifierFlags.intersection([.command, .control, .option]).isEmpty {
            finishSession(client); return false
        }
        if inline.isEmpty && !sentence.isEmpty {
            let range = selectedRange(client)
            if sentence.expired(now: now) || !sentence.isAtExpectedCaret(location(range), selectionLength: range.length == NSNotFound ? 0 : range.length) { reset() }
        }
        sentence.noteActivity(now: now)
        switch event.keyCode {
        case 53:
            guard !inline.isEmpty || !sentence.isEmpty else { return false }
            if !inline.isEmpty { sendMarkedText("", to: client) }
            reset(); return true
        case 51:
            if !raw.isEmpty { raw.removeLast() }
            else if !locked.isEmpty { locked.removeLast() }
            else if !sentence.isEmpty {
                sentence.deleteLast(now: now)
                if sentence.isEmpty { reset() } else { requestTranslation(); startTracking() }
                return false
            } else { return false }
            selection = 0; refresh(client); return true
        case 36, 76:
            guard !inline.isEmpty else {
                if !sentence.isEmpty { sentence.finish(now: now); requestTranslation(); updatePosition(client) }
                return false
            }
            return commit(client, english: Preferences.shared.englishDefault)
        case 48:
            guard !inline.isEmpty else { reset(); return false }
            return commit(client, english: !Preferences.shared.englishDefault)
        case 49:
            guard !inline.isEmpty else {
                if !sentence.isEmpty { sentence.append(" ", startingAt: nil, now: now); requestTranslation(); startTracking() }
                return false
            }
            if raw.isEmpty { return commit(client, english: Preferences.shared.englishDefault) }
            if choices.isEmpty { commitLiteral(client); return false }
            choose(selection, client: client); return true
        default: break
        }
        if [123, 124, 125, 126, 115, 119, 116, 121].contains(event.keyCode) {
            guard !raw.isEmpty else { finishSession(client); return false }
            let movement = [123: -1, 126: -1, 124: 1, 125: 1, 116: -5, 121: 5][Int(event.keyCode)] ?? 0
            selection = max(0, min(choices.count - 1, selection + movement))
            requestTranslation(); updatePosition(client); return true
        }
        guard let characters = event.characters, !characters.isEmpty else { return false }
        if let number = Int(characters), (1...5).contains(number), !raw.isEmpty {
            let index = (selection / 5) * 5 + number - 1
            if choices.indices.contains(index) { choose(index, client: client); return true }
        }
        guard Self.engine.available else { commitLiteral(client); return false }
        if characters.unicodeScalars.allSatisfy({ (97...122).contains($0.value) }) || (characters == "'" && !raw.isEmpty) {
            if inline.isEmpty {
                let range = selectedRange(client)
                sentence.beginInput(at: location(range), selectionLength: range.length == NSNotFound ? 0 : range.length, now: now)
            }
            guard raw.count < 100, locked.count < 500 else { NSSound.beep(); return true }
            raw += characters; selection = 0; refresh(client); return true
        }
        let punctuation = [",": "，", ".": "。", "?": "？", "!": "！", ";": "；", ":": "："]
        if let mark = punctuation[characters], !inline.isEmpty || !sentence.isEmpty {
            if Preferences.shared.englishDefault && !inline.isEmpty && completeFragment {
                locked = fragment + mark; raw = ""; selection = 0; refresh(client)
            } else { commitChinese(client, suffix: mark) }
            return true
        }
        if !inline.isEmpty { commitChinese(client) }
        if !sentence.isEmpty && characters.unicodeScalars.allSatisfy({ $0.value >= 32 && $0.value < 127 }) {
            sentence.append(characters, startingAt: nil, now: now); requestTranslation(); startTracking()
        } else if !sentence.isEmpty { reset() }
        return false
    }
    private func choose(_ index: Int, client: IMKTextInput) {
        let items = choices
        guard items.indices.contains(index) else { return }
        locked += items[index].text; raw.removeFirst(items[index].code.count); selection = 0
        if raw.isEmpty && !Preferences.shared.englishDefault { commitChinese(client) }
        else { refresh(client) }
    }
    private func commit(_ client: IMKTextInput, english: Bool) -> Bool {
        guard !PrivacyProtection.isBlocking else { reset(); return true }
        if !english { commitChinese(client); return true }
        guard completeFragment, translatedSource == source, !translation.isEmpty else {
            translationStatus = completeFragment ? "整句译文尚未就绪，请稍候" : "请先补全拼音或选词"
            FloatingPanel.shared.model.status = translationStatus; updatePosition(client); return true
        }
        var range = replacement
        if !sentence.isEmpty {
            guard let tracked = sentence.replacementRange(markedRange: markedRange(client)) else {
                translationStatus = "当前应用无法安全替换整句，请输入中文"
                FloatingPanel.shared.model.english = ""; FloatingPanel.shared.model.status = translationStatus; updatePosition(client); return true
            }
            range = tracked
        }
        let output = translation
        reset(); sendText(output, to: client, range: range); return true
    }
    private func sendText(_ text: String, to client: IMKTextInput, range: NSRange) {
        clientUpdateDepth += 1; defer { clientUpdateDepth -= 1 }
        client.insertText(text, replacementRange: range)
    }
    private func sendMarkedText(_ text: String, to client: IMKTextInput) {
        clientUpdateDepth += 1; defer { clientUpdateDepth -= 1 }
        client.setMarkedText(text, selectionRange: NSRange(location: text.utf16.count, length: 0), replacementRange: replacement)
    }
    private func clearComposition() {
        raw = ""; locked = ""; selection = 0; Self.engine.clearActiveQuery()
        if Self.activeController === self || Self.activeController == nil { FloatingPanel.shared.onChoose = nil }
    }
    private func commitChinese(_ client: IMKTextInput, suffix: String = "") {
        guard !PrivacyProtection.isBlocking else { reset(); return }
        let text = (completeFragment ? fragment : inline) + suffix
        guard !text.isEmpty else { return }
        let marked = markedRange(client)
        let start = location(marked) ?? location(selectedRange(client))
        sentence.append(text, startingAt: start, now: now)
        clearComposition(); markedText = ""
        sendText(text, to: client, range: replacement)
        needsFreshAnchor = true
        startTracking(); requestTranslation()
        DispatchQueue.main.async { [weak self] in
            guard let self, self.isActive, let current = self.activeClient else { return }; self.updatePosition(current)
        }
    }
    private func refresh(_ client: IMKTextInput) {
        guard !PrivacyProtection.isBlocking else { reset(); return }
        if markedText != inline {
            markedText = inline
            sendMarkedText(inline, to: client)
        }
        guard !inline.isEmpty || !sentence.isEmpty else { reset(); return }
        FloatingPanel.shared.onChoose = { [weak self] index in
            guard let self, self.isActive, !PrivacyProtection.isBlocking, let current = self.activeClient else { return }; self.choose(index, client: current)
        }
        modeToast?.cancel(); modeToast = nil
        startTracking(); requestTranslation()
        DispatchQueue.main.async { [weak self] in
            guard let self, self.isActive, let current = self.activeClient else { return }; self.updatePosition(current)
        }
    }
    private func startTracking() {
        guard trackingTimer == nil else { return }
        let timer = Timer(timeInterval: 0.1, target: self, selector: #selector(track(_:)), userInfo: nil, repeats: true)
        RunLoop.main.add(timer, forMode: .common); trackingTimer = timer
    }
    private func requestTranslation() {
        let current = source
        let bubble = FloatingPanel.shared
        bubble.model.source = current; bubble.model.pinyin = raw; bubble.model.candidates = choices; bubble.model.selected = selection
        // Selecting a word must not cancel an identical in-flight sentence translation.
        if !current.isEmpty && current == requestedSource {
            bubble.model.english = translation; bubble.model.status = translationStatus; return
        }
        revision += 1; let ticket = revision
        work?.cancel(); work = nil; translation = ""; translatedSource = ""; requestedSource = current
        bubble.model.english = ""
        guard !current.isEmpty else {
            translationStatus = Self.engine.available ? "补全拼音或选择中文后显示翻译" : "本地拼音引擎未就绪，请重新安装"
            bubble.model.status = translationStatus; return
        }
        translationStatus = "正在翻译整句…"; bubble.model.status = translationStatus
        work = Task { [weak self] in
            do {
                try await Task.sleep(for: .milliseconds(280))
                let result = try await TranslationService.shared.english(for: current)
                guard let self, self.isActive, !Task.isCancelled, self.revision == ticket else { return }
                guard !PrivacyProtection.isBlocking, !Preferences.shared.directEnglish else { self.reset(); return }
                self.translation = result; self.translatedSource = current
                bubble.model.english = result
                if let current = self.activeClient { self.updatePosition(current) }
            } catch {
                guard let self, self.isActive, !Task.isCancelled, self.revision == ticket else { return }
                guard !PrivacyProtection.isBlocking, !Preferences.shared.directEnglish else { self.reset(); return }
                self.translationStatus = "本机翻译未就绪，请在设置检查语言包"
                bubble.model.status = self.translationStatus
                if let current = self.activeClient { self.updatePosition(current) }
            }
        }
    }
    @objc private func track(_ timer: Timer) {
        guard isActive else { reset(); return }
        guard !PrivacyProtection.isBlocking else { reset(); return }
        if FloatingPanel.shared.isSelectingTranslation { return }
        if Preferences.shared.directEnglish { if let client = activeClient { commitLiteral(client) } else { reset() }; return }
        if sentence.expired(now: now) { reset(); return }
        if inline.isEmpty && !sentence.isEmpty, let client = activeClient {
            let range = selectedRange(client)
            if !sentence.isAtExpectedCaret(location(range), selectionLength: range.length == NSNotFound ? 0 : range.length) { reset(); return }
        }
        if let client = activeClient { updatePosition(client) }
    }
    private func updatePosition(_ client: IMKTextInput) {
        positionGeneration &+= 1
        let ticket = positionGeneration
        DispatchQueue.main.async { [weak self] in
            guard let self, self.isActive, self.positionGeneration == ticket,
                  let current = self.activeClient else { return }
            self.displayPosition(current)
        }
    }
    private func displayPosition(_ client: IMKTextInput) {
        guard isActive, !PrivacyProtection.isBlocking, !inline.isEmpty || !sentence.isEmpty else { return }
        if FloatingPanel.shared.isSelectingTranslation { return }
        if !isChromium || (needsFreshAnchor && now - lastGeometryAttempt >= 1) {
            lastGeometryAttempt = now
            var caret = NSRect.zero
            _ = client.attributes(forCharacterIndex: 0, lineHeightRectangle: &caret)
            // Do not trigger a second synchronous Chrome renderer request on a miss.
            if !isChromium && !PanelPlacement.isValidAnchor(caret) {
                let marked = markedRange(client); let range = marked.location == NSNotFound ? selectedRange(client) : marked
                if range.location != NSNotFound { caret = client.firstRect(forCharacterRange: NSRange(location: range.location, length: 0), actualRange: nil) }
            }
            if PanelPlacement.isValidAnchor(caret) { lastCaret = caret; needsFreshAnchor = false }
        }
        if PanelPlacement.isValidAnchor(lastCaret) { FloatingPanel.shared.show(near: lastCaret, clientLevel: isChromium ? 0 : client.windowLevel()) }
        else { FloatingPanel.shared.conceal() }
    }
    private func commitLiteral(_ client: IMKTextInput) {
        guard !PrivacyProtection.isBlocking else { reset(); return }
        let text = inline; reset()
        if !text.isEmpty { sendText(text, to: client, range: replacement) }
    }
    private func finishSession(_ client: IMKTextInput, preservingSelection: Bool = false) {
        if !inline.isEmpty { commitChinese(client) }
        reset(preservingSelection: preservingSelection)
    }
    private func showModeToast(_ client: IMKTextInput) {
        let bubble = FloatingPanel.shared
        bubble.model.status = Preferences.shared.directEnglish ? "英文输入 · Shift 切换中文" : "中文输入 · Shift 切换英文"
        positionGeneration &+= 1; let ticket = positionGeneration
        DispatchQueue.main.async { [weak self] in
            guard let self, self.isActive, self.positionGeneration == ticket, let client = self.activeClient else { return }
            var caret = self.lastCaret
            if !self.isChromium { _ = client.attributes(forCharacterIndex: 0, lineHeightRectangle: &caret) }
            if PanelPlacement.isValidAnchor(caret) { bubble.show(near: caret, clientLevel: 0) }
        }
        modeToast?.cancel()
        modeToast = Task { try? await Task.sleep(for: .seconds(1)); guard !Task.isCancelled else { return }; bubble.hide() }
    }
    private func reset(preservingSelection: Bool = false) {
        positionGeneration &+= 1; needsFreshAnchor = true; lastGeometryAttempt = -.infinity
        revision += 1; work?.cancel(); work = nil; modeToast?.cancel(); modeToast = nil
        trackingTimer?.invalidate(); trackingTimer = nil
        clearComposition(); markedText = ""; sentence.clear(); translation = ""; translatedSource = ""; requestedSource = ""; translationStatus = ""; lastCaret = .zero
        if Self.activeController === self || Self.activeController == nil {
            if preservingSelection && !PrivacyProtection.isBlocking { FloatingPanel.shared.keepSelectionForCopying() }
            else { FloatingPanel.shared.hide() }
        }
    }
    func flush(_ sender: Any!) {
        guard clientUpdateDepth == 0, isActive, let client = (sender as? IMKTextInput) ?? activeClient else { return }
        finishSession(client, preservingSelection: FloatingPanel.shared.isSelectingTranslation)
    }
    func end(_ sender: Any!, allowSelection: Bool = true) {
        shiftTap.reset()
        guard Self.activeController === self else { activeClient = nil; return }
        let keepSelection = allowSelection && FloatingPanel.shared.isSelectingTranslation
        if let client = (sender as? IMKTextInput) ?? activeClient { finishSession(client, preservingSelection: keepSelection) }
        else { reset(preservingSelection: keepSelection) }
        activeClient = nil; Self.activeController = nil
    }
    func conceal() {
        if Self.activeController === self { FloatingPanel.shared.conceal() }
    }
}
