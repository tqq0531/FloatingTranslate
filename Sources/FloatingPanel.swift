import AppKit
import SwiftUI

@MainActor
final class BubbleModel: ObservableObject {
    @Published var toChinese = false
    @Published var source = ""
    @Published var english = ""
    @Published var status = ""
    @Published var pinyin = ""
    @Published var candidates: [PinyinCandidate] = []
    @Published var selected = 0
}

struct TranslationBubble: View {
    @ObservedObject var model: BubbleModel
    @ObservedObject var preferences = Preferences.shared
    var width: CGFloat = 520

    private var englishLine: some View {
        HStack(alignment: .top, spacing: 9) {
            Text(model.toChinese ? "中" : "EN").font(.system(size: 10, weight: .semibold, design: .rounded))
                .foregroundStyle(.white.opacity(0.5)).padding(.top, 4)
            Text(model.english.isEmpty ? model.status : model.english)
                .font(.system(size: 17, weight: .medium))
                .foregroundStyle(model.english.isEmpty ? .white.opacity(0.5) : .white.opacity(0.95))
                .textSelection(.enabled)
                .fixedSize(horizontal: false, vertical: true).lineLimit(3)
                .frame(maxWidth: .infinity, alignment: .leading)
        }.padding(.horizontal, 14).padding(.vertical, 11)
    }
    private var candidateLine: some View {
        VStack(alignment: .leading, spacing: 7) {
            Text(model.pinyin.isEmpty ? model.source : model.pinyin)
                .font(.system(size: 15)).foregroundStyle(.white.opacity(0.9))
                .lineLimit(1).padding(.horizontal, 12)
            if !model.pinyin.isEmpty && !model.candidates.isEmpty {
                HStack(spacing: 2) {
                    ForEach(Array(model.candidates.prefix(5).enumerated()), id: \.offset) { index, item in
                        HStack(spacing: 5) {
                            Text("\(index + 1)").font(.system(size: 10, weight: .medium))
                                .opacity(index == model.selected ? 1 : 0.5)
                            Text(item.text).font(.system(size: 19, weight: .medium)).lineLimit(1)
                        }
                        .foregroundStyle(.white)
                        .padding(.horizontal, 9).padding(.vertical, 5)
                        .background(index == model.selected ? Color.blue : Color.clear, in: Capsule())
                    }
                    Spacer(minLength: 0)
                }.padding(.horizontal, 5)
            }
        }.padding(.vertical, 8)
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            if preferences.translationAbove { englishLine }
            if preferences.translationAbove { Divider().overlay(.white.opacity(0.08)) }
            candidateLine
            if !preferences.translationAbove { Divider().overlay(.white.opacity(0.08)); englishLine }
        }
        .frame(width: width, alignment: .leading)
        .background(Color(white: 0.16), in: RoundedRectangle(cornerRadius: 16))
        .overlay(RoundedRectangle(cornerRadius: 16).stroke(.white.opacity(0.2)))
        .colorScheme(.dark)
        .padding(8)
    }
}

@MainActor
private final class CandidateWindow: NSPanel {
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { false }
}

/// Read-only text selection in a nonactivating panel; no pasteboard reads.
@MainActor
final class TranslationSelectionView: NSTextView {
    var translationReady = false
    private(set) var draggingSelection = false
    override var needsPanelToBecomeKey: Bool { true }
    // Selecting a read-only translation must not activate our own IME recursively.
    override var inputContext: NSTextInputContext? { nil }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }
    override func mouseDown(with event: NSEvent) {
        guard translationReady, !PrivacyProtection.isBlocking else { return }
        draggingSelection = true
        defer { draggingSelection = false }
        window?.makeKey()
        window?.makeFirstResponder(self)
        super.mouseDown(with: event)
    }
    @discardableResult func copyTranslation(to pasteboard: NSPasteboard, selectionOnly: Bool) -> Bool {
        guard translationReady, !PrivacyProtection.isBlocking, !string.isEmpty else { return false }
        let range = selectedRange()
        if selectionOnly && range.length == 0 { return false }
        let output = selectionOnly ? (string as NSString).substring(with: range) : string
        pasteboard.clearContents()
        return pasteboard.setString(output, forType: .string)
    }
    private func handleSelectionShortcut(_ event: NSEvent) -> Bool {
        guard event.modifierFlags.intersection([.command, .control, .option]) == .command else { return false }
        switch event.charactersIgnoringModifiers?.lowercased() {
        case "c": copy(nil); return true
        case "a": selectAll(nil); return true
        default: return false
        }
    }
    override func performKeyEquivalent(with event: NSEvent) -> Bool {
        handleSelectionShortcut(event) || super.performKeyEquivalent(with: event)
    }
    override func keyDown(with event: NSEvent) {
        if !handleSelectionShortcut(event) { super.keyDown(with: event) }
    }
    override func copy(_ sender: Any?) { _ = copyTranslation(to: .general, selectionOnly: true) }
    @objc private func copyWholeTranslation(_ sender: Any?) { _ = copyTranslation(to: .general, selectionOnly: false) }
    override func menu(for event: NSEvent) -> NSMenu? {
        guard translationReady, !PrivacyProtection.isBlocking else { return nil }
        let menu = NSMenu(); menu.autoenablesItems = false
        let selection = NSMenuItem(title: "复制选中英文", action: #selector(copy(_:)), keyEquivalent: "")
        selection.target = self; selection.isEnabled = selectedRange().length > 0; menu.addItem(selection)
        let all = NSMenuItem(title: "复制整句译文", action: #selector(copyWholeTranslation(_:)), keyEquivalent: "")
        all.target = self; menu.addItem(all); return menu
    }
}

@MainActor
final class FloatingPanel: NSObject, NSWindowDelegate {
    static let shared = FloatingPanel()
    let model = BubbleModel()
    var onChoose: ((Int) -> Void)?
    private let panel: NSPanel
    private let content = NSView()
    private let background = NSVisualEffectView()
    let translationText = TranslationSelectionView(frame: .zero)
    private let copyButton = NSButton()
    private let badge = NSTextField(labelWithString: "EN")
    private let separator = NSBox()
    private var buttons: [NSButton] = []
    private var rendered: RenderState?
    private var selectionWatch: Timer?
    private var selectionDeadline: TimeInterval = 0
    private var lastCaret = NSRect.zero
    private var lastLevel: Int32 = 0
    private struct RenderState: Equatable {
        let text: String
        let ready: Bool
        let choices: [PinyinCandidate]
        let selected: Int
        let above: Bool
        let toChinese: Bool
    }
    var isVisible: Bool { panel.isVisible }
    var renderedEnglish: String { translationText.string }
    var renderedCandidateCount: Int { buttons.count }
    override init() {
        panel = CandidateWindow(contentRect: NSRect(x: 0, y: 0, width: 300, height: 84), styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
        super.init()
        panel.delegate = self
        panel.level = .popUpMenu
        panel.isOpaque = false; panel.backgroundColor = .clear; panel.hasShadow = true
        panel.hidesOnDeactivate = false; panel.becomesKeyOnlyIfNeeded = true
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        background.material = .popover; background.blendingMode = .behindWindow; background.state = .active
        background.wantsLayer = true; background.layer?.cornerRadius = 12; background.layer?.masksToBounds = true
        // Keep the window backing transparent; mask the material itself as well.
        // A layer cornerRadius alone does not clip behind-window vibrancy.
        content.wantsLayer = true
        content.layer?.backgroundColor = NSColor.clear.cgColor
        content.layer?.cornerRadius = 12
        content.layer?.masksToBounds = true
        panel.contentView = content
        background.frame = content.bounds
        background.autoresizingMask = [.width, .height]
        content.addSubview(background)
        translationText.font = .systemFont(ofSize: 15, weight: .medium)
        translationText.isEditable = false; translationText.isSelectable = true
        translationText.isRichText = false; translationText.drawsBackground = false
        translationText.textContainerInset = .zero
        translationText.textContainer?.lineFragmentPadding = 0
        translationText.textContainer?.maximumNumberOfLines = 3
        translationText.textContainer?.lineBreakMode = .byTruncatingTail
        translationText.isVerticallyResizable = false; translationText.isHorizontallyResizable = false
        copyButton.image = NSImage(systemSymbolName: "doc.on.doc", accessibilityDescription: "复制整句译文")
        copyButton.target = self; copyButton.action = #selector(copyWhole(_:))
        copyButton.isBordered = false; copyButton.refusesFirstResponder = true
        copyButton.toolTip = "复制整句译文"
        badge.font = .systemFont(ofSize: 9, weight: .semibold); badge.textColor = .secondaryLabelColor
        separator.boxType = .separator
        for view in [translationText, badge, separator, copyButton] { background.addSubview(view) }
    }
    func relayout() { show(near: lastCaret, clientLevel: lastLevel) }
    func show(near caret: NSRect, clientLevel: Int32 = 0) {
        guard !PrivacyProtection.isBlocking else { hide(); return }
        if isSelectingTranslation { return }
        guard PanelPlacement.isValidAnchor(caret), let screen = NSScreen.screens.first(where: { $0.visibleFrame.intersects(caret) }) else { conceal(); return }
        lastCaret = caret; lastLevel = clientLevel
        let state = RenderState(text: model.english.isEmpty ? (model.status.isEmpty ? "输入完整拼音后显示英文" : model.status) : model.english,
                                ready: !model.english.isEmpty, choices: model.candidates, selected: model.selected,
                                above: Preferences.shared.translationAbove, toChinese: model.toChinese)
        if state != rendered { render(state, screenWidth: screen.visibleFrame.width) }
        panel.level = NSWindow.Level(rawValue: max(NSWindow.Level.popUpMenu.rawValue, Int(clientLevel) + 1))
        guard let origin = PanelPlacement.origin(anchor: caret, size: panel.frame.size, visibleFrame: screen.visibleFrame, preferAbove: false) else { conceal(); return }
        panel.setFrameOrigin(origin)
        panel.orderFrontRegardless()
    }
    private func render(_ state: RenderState, screenWidth: CGFloat) {
        rendered = state
        badge.stringValue = state.toChinese ? "中" : "EN"
        let maxWidth = min(580, screenWidth - 16)
        let page = Array(state.choices.enumerated().dropFirst((state.selected / 5) * 5).prefix(5))
        let widths = page.map { min(165, max(44, ($0.element.text as NSString).size(withAttributes: [.font: NSFont.systemFont(ofSize: 19)]).width + 30)) }
        let rowWidth = widths.reduce(0, +) + CGFloat(max(0, page.count - 1)) * 4 + 16
        let desiredEnglish = (state.text as NSString).size(withAttributes: [.font: translationText.font!]).width + 80
        let width = min(maxWidth, max(260, max(min(desiredEnglish, 460), rowWidth)))
        let textHeight = (state.text as NSString).boundingRect(with: NSSize(width: width - 80, height: 70), options: [.usesLineFragmentOrigin, .usesFontLeading], attributes: [.font: translationText.font!]).height
        let englishHeight = min(60, max(20, ceil(textHeight)))
        let englishBand = englishHeight + 18
        let candidateBand: CGFloat = page.isEmpty ? 0 : 40
        let height = englishBand + candidateBand
        panel.setContentSize(NSSize(width: width, height: height))
        background.frame = content.bounds
        background.maskImage = NSImage(size: content.bounds.size, flipped: false) { rect in
            NSColor.black.setFill()
            NSBezierPath(roundedRect: rect, xRadius: 12, yRadius: 12).fill()
            return true
        }
        panel.invalidateShadow()
        let englishBottom = state.above ? candidateBand + 9 : 9
        translationText.frame = NSRect(x: 36, y: englishBottom, width: width - 80, height: englishHeight)
        if translationText.string != state.text { translationText.string = state.text; finishTextSelection() }
        translationText.translationReady = state.ready
        copyButton.isEnabled = state.ready
        copyButton.frame = NSRect(x: width - 32, y: englishBottom + englishHeight - 22, width: 24, height: 24)
        translationText.textColor = state.ready ? .labelColor : .secondaryLabelColor
        badge.frame = NSRect(x: 11, y: englishBottom + englishHeight - 15, width: 22, height: 13)
        separator.isHidden = page.isEmpty
        separator.frame = NSRect(x: 8, y: state.above ? candidateBand : englishBand, width: width - 16, height: 1)
        buttons.forEach { $0.removeFromSuperview() }; buttons = []
        var x: CGFloat = 8
        let rowBottom: CGFloat = state.above ? 5 : englishBand + 5
        for (offset, item) in page.enumerated() {
            let buttonWidth = min(widths[offset], width - x - 8)
            guard buttonWidth >= 36 else { break }
            let button = NSButton(title: "", target: self, action: #selector(choose(_:)))
            button.tag = item.offset; button.isBordered = false; button.refusesFirstResponder = true
            button.frame = NSRect(x: x, y: rowBottom, width: buttonWidth, height: 30)
            button.wantsLayer = true; button.layer?.cornerRadius = 13
            let selected = item.offset == state.selected
            button.layer?.backgroundColor = selected ? NSColor.controlAccentColor.cgColor : NSColor.clear.cgColor
            let color: NSColor = selected ? .white : .labelColor
            let title = NSMutableAttributedString(string: "\(offset + 1) ", attributes: [.font: NSFont.systemFont(ofSize: 10), .foregroundColor: color])
            title.append(NSAttributedString(string: item.element.text, attributes: [.font: NSFont.systemFont(ofSize: 19, weight: .medium), .foregroundColor: color]))
            button.attributedTitle = title
            button.cell?.lineBreakMode = .byTruncatingTail
            background.addSubview(button); buttons.append(button)
            x += buttonWidth + 4
        }
    }
    var isSelectingTranslation: Bool { translationText.draggingSelection || translationText.selectedRange().length > 0 }
    func finishTextSelection() {
        translationText.setSelectedRange(NSRange(location: 0, length: 0))
        selectionWatch?.invalidate(); selectionWatch = nil
    }
    func keepSelectionForCopying() {
        guard translationText.translationReady, !PrivacyProtection.isBlocking else { hide(); return }
        onChoose = nil
        buttons.forEach { $0.isEnabled = false }
        selectionDeadline = ProcessInfo.processInfo.systemUptime + 15
        selectionWatch?.invalidate()
        let timer = Timer(timeInterval: 0.1, target: self, selector: #selector(checkSelectionPrivacy(_:)), userInfo: nil, repeats: true)
        RunLoop.main.add(timer, forMode: .common); selectionWatch = timer
    }
    @objc private func checkSelectionPrivacy(_ timer: Timer) {
        if PrivacyProtection.isBlocking || ProcessInfo.processInfo.systemUptime >= selectionDeadline { hide() }
    }
    func windowDidResignKey(_ notification: Notification) {
        if selectionWatch != nil { hide() }
        else { finishTextSelection() }
    }
    @discardableResult func copyEnglish(to pasteboard: NSPasteboard) -> Bool { translationText.copyTranslation(to: pasteboard, selectionOnly: false) }
    @objc private func copyWhole(_ sender: Any?) { _ = copyEnglish(to: .general) }
    @objc private func choose(_ sender: NSButton) { finishTextSelection(); onChoose?(sender.tag) }
    func conceal() { panel.orderOut(nil) }
    func hide() {
        conceal(); onChoose = nil; rendered = nil
        model.source = ""; model.english = ""; model.status = ""
        model.pinyin = ""; model.candidates = []; model.selected = 0
        finishTextSelection(); translationText.translationReady = false
        translationText.string = ""; buttons.forEach { $0.removeFromSuperview() }; buttons = []
    }
    func savePreview(to url: URL) throws {
        guard let bitmap = background.bitmapImageRepForCachingDisplay(in: background.bounds) else { return }
        background.cacheDisplay(in: background.bounds, to: bitmap)
        if let data = bitmap.representation(using: .png, properties: [:]) { try data.write(to: url) }
    }
}
