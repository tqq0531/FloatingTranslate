import AppKit

@main struct PanelSmoke {
    @MainActor static func main() async throws {
        _ = NSApplication.shared
        guard !PrivacyProtection.isBlocking else { print("SKIP panel test: privacy or secure input is active"); return }
        let engine = PinyinEngine(url: URL(fileURLWithPath: CommandLine.arguments[1]))
        let panel = FloatingPanel.shared
        defer { panel.hide() }
        var sentence = SentenceBuffer()
        sentence.append("我", startingAt: 0, now: 1)
        sentence.append("今天", startingAt: 1, now: 2)
        sentence.append("去北京", startingAt: 3, now: 3)
        let translated = try await TranslationService.shared.english(for: sentence.text)
        panel.model.source = sentence.text; panel.model.pinyin = "qubeijing"
        panel.model.candidates = engine.candidates("qubeijing")
        panel.model.status = "正在翻译…"
        guard let screen = NSScreen.main else { fatalError("No display") }
        let anchor = NSRect(x: screen.visibleFrame.minX + 180, y: screen.visibleFrame.midY, width: 1, height: 22)
        panel.show(near: anchor)
        guard panel.isVisible, panel.renderedEnglish == "正在翻译…", panel.renderedCandidateCount > 0 else { fatalError("Translation status and candidates must be visible together") }
        print("PASS translation status and candidates share one visible window")
        let testPasteboard = NSPasteboard.withUniqueName()
        defer { testPasteboard.releaseGlobally() }
        guard !panel.copyEnglish(to: testPasteboard) else { fatalError("Waiting status must not be copied as English") }
        panel.model.english = translated
        panel.show(near: anchor)
        try await Task.sleep(for: .milliseconds(350))
        guard panel.isVisible, panel.renderedEnglish == translated, panel.renderedCandidateCount > 0 else { fatalError("English must remain visible with candidates") }
        print("PASS real local translation is rendered in the candidate window: \(translated)")
        guard panel.translationText.inputContext == nil else { fatalError("Selecting read-only English must not start another IME session") }
        guard panel.translationText.isSelectable, !panel.translationText.isEditable,
              panel.translationText.window?.canBecomeKey == true else { fatalError("English must support mouse selection in a key-capable nonactivating panel") }
        guard panel.copyEnglish(to: testPasteboard), testPasteboard.string(forType: .string) == translated else { fatalError("Copy whole English failed") }
        let selected = NSRange(location: 0, length: 3)
        panel.translationText.setSelectedRange(selected)
        guard panel.translationText.copyTranslation(to: testPasteboard, selectionOnly: true),
              testPasteboard.string(forType: .string) == (translated as NSString).substring(with: selected) else { fatalError("Copy selected English failed") }
        let shown = panel.renderedEnglish
        panel.model.english = "A newer translation."
        panel.show(near: anchor.offsetBy(dx: 100, dy: 0))
        guard panel.renderedEnglish == shown && panel.translationText.selectedRange() == selected else { fatalError("Live updates interrupted selection") }
        panel.finishTextSelection(); panel.model.english = translated
        print("PASS mouse-selectable read-only English supports copying selection and full sentence without interrupting selection")
        panel.model.toChinese = true
        panel.model.english = "你今天锻炼了吗？"; panel.model.candidates = []
        panel.show(near: anchor)
        guard panel.copyEnglish(to: testPasteboard), testPasteboard.string(forType: .string) == "你今天锻炼了吗？" else { fatalError("Chinese translation copy failed") }
        panel.translationText.setSelectedRange(NSRange(location: 0, length: 2))
        guard panel.translationText.copyTranslation(to: testPasteboard, selectionOnly: true), testPasteboard.string(forType: .string) == "你今" else { fatalError("Chinese selected copy failed") }
        panel.finishTextSelection(); panel.model.toChinese = false
        print("PASS Chinese translation supports whole and selected copy")
        // Inspect the actual composited window, including the vibrancy backing.
        // cacheDisplay alone cannot catch rectangular behind-window material.
        guard let window = panel.translationText.window else { fatalError("No candidate window") }
        for appearance in [NSAppearance.Name.aqua, .darkAqua] {
            window.appearance = NSAppearance(named: appearance)
            for candidates in [engine.candidates("qubeijing"), []] {
                panel.model.candidates = candidates
                panel.model.english = candidates.isEmpty ? "Hello" : translated
                panel.show(near: anchor)
                window.displayIfNeeded()
                try await Task.sleep(for: .milliseconds(250))
                let url = URL(fileURLWithPath: CommandLine.arguments[2]).deletingLastPathComponent()
                    .appendingPathComponent("panel-\(appearance.rawValue)-\(candidates.isEmpty ? "compact" : "candidates").png")
                let capture = Process()
                capture.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
                capture.arguments = ["-x", "-o", "-l", String(window.windowNumber), url.path]
                try capture.run(); capture.waitUntilExit()
                guard capture.terminationStatus == 0,
                      let bitmap = NSBitmapImageRep(data: try Data(contentsOf: url)) else { fatalError("Cannot capture candidate window") }
                let w = bitmap.pixelsWide, h = bitmap.pixelsHigh
                for (x, y) in [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)] {
                    guard let pixel = bitmap.colorAt(x: x, y: y), pixel.alphaComponent < 0.02 else {
                        fatalError("Candidate window corner must be transparent: \(appearance.rawValue), \(w)x\(h), \(x),\(y)")
                    }
                }
                guard let center = bitmap.colorAt(x: w / 2, y: h / 2), center.alphaComponent > 0.5 else {
                    fatalError("Candidate window body must remain visible")
                }
            }
        }
        window.appearance = nil
        panel.model.english = translated
        print("PASS all four composited corners are transparent in light/dark appearance, with candidates and after resizing")
        panel.model.candidates = []; panel.model.pinyin = ""
        panel.show(near: anchor)
        guard panel.isVisible, panel.renderedEnglish == translated, panel.renderedCandidateCount == 0 else { fatalError("Committed sentence translation must remain visible without active candidates") }
        print("PASS full sentence translation remains visible after all words are committed")
        try panel.savePreview(to: URL(fileURLWithPath: CommandLine.arguments[2]))
        panel.hide()
        guard !panel.copyEnglish(to: testPasteboard), panel.translationText.selectedRange().length == 0 else { fatalError("Hidden translation must not be copyable") }
        guard panel.renderedEnglish.isEmpty, panel.renderedCandidateCount == 0 else { fatalError("Panel retained input after closing") }
        print("PASS closed candidate window clears displayed text")
    }
}
