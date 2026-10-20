import Foundation

/// Only the current sentence composed through this IME, never surrounding text.
struct SentenceBuffer {
    private(set) var text = ""
    private(set) var startLocation: Int?
    private(set) var finished = false
    private var touchedAt: TimeInterval = 0
    var expectedCaret: Int? { startLocation.map { $0 + text.utf16.count } }
    var isEmpty: Bool { text.isEmpty }
    mutating func clear() { text = ""; startLocation = nil; finished = false; touchedAt = 0 }
    mutating func beginInput(at location: Int?, selectionLength: Int = 0, now: TimeInterval) {
        if finished || expired(now: now) || selectionLength > 0 || (location != nil && expectedCaret != nil && location != expectedCaret) { clear() }
        touchedAt = now
    }
    mutating func append(_ fragment: String, startingAt location: Int?, now: TimeInterval) {
        if finished { clear() }
        if text.isEmpty { startLocation = location }
        text += fragment
        if text.count > 1000 { clear(); return }
        finished = text.last.map { "。！？.!?\n".contains($0) } ?? false
        touchedAt = now
    }
    mutating func deleteLast(now: TimeInterval) {
        guard !text.isEmpty else { return }
        text.removeLast(); finished = false; touchedAt = now
        if text.isEmpty { clear() }
    }
    // Preserve this IME's sentence for translation, but prohibit range replacement
    // once the host can no longer provide a trustworthy document position.
    mutating func discardDocumentPosition() { startLocation = nil }
    mutating func noteActivity(now: TimeInterval) { if !isEmpty { touchedAt = now } }
    mutating func finish(now: TimeInterval) { finished = true; touchedAt = now }
    func expired(now: TimeInterval) -> Bool { !text.isEmpty && now - touchedAt > (finished ? 8 : 60) }
    func isAtExpectedCaret(_ location: Int?, selectionLength: Int = 0) -> Bool {
        selectionLength == 0 && (location == nil || expectedCaret == nil || location == expectedCaret)
    }
    func replacementRange(markedRange: NSRange) -> NSRange? {
        guard let startLocation, let expectedCaret, markedRange.location == expectedCaret else { return nil }
        return NSRange(location: startLocation, length: text.utf16.count + markedRange.length)
    }
}
