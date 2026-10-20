import Foundation

struct PinyinCandidate: Equatable {
    let text: String
    /// Exact original keystrokes consumed, including abbreviations and typos.
    let code: String
    let weight: Double
    let correction: String
    init(text: String, code: String, weight: Double, correction: String = "") {
        self.text = text; self.code = code; self.weight = weight; self.correction = correction
    }
}

/// Serial on-device Rime adapter. User dictionaries and learning are disabled.
final class PinyinEngine {
    private(set) var available = false
    private(set) var count = 65_125
    private var lastRaw: String?
    private var lastCandidates: [PinyinCandidate] = []
    init(url: URL?) {
        let shared = Bundle.main.resourceURL?.appendingPathComponent("RimeData")
        let fallback = url?.deletingLastPathComponent().appendingPathComponent("RimeData")
        let data = [shared, fallback].compactMap { $0 }.first { FileManager.default.fileExists(atPath: $0.appendingPathComponent("build/fuyi.schema.yaml").path) }
        guard let data else { return }
        let runtime = FileManager.default.temporaryDirectory.appendingPathComponent("fuyi-rime-\(ProcessInfo.processInfo.processIdentifier)")
        do { try FileManager.default.createDirectory(at: runtime, withIntermediateDirectories: true) } catch { return }
        available = FuyiRimeInitialize(data.path, runtime.path) != 0
    }
    var version: String { String(cString: FuyiRimeVersion()) }
    func candidates(_ raw: String) -> [PinyinCandidate] {
        guard available, !raw.isEmpty, raw.count <= 100 else { return [] }
        if lastRaw == raw { return lastCandidates }
        let capacity = 30
        let texts = UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>.allocate(capacity: capacity)
        texts.initialize(repeating: nil, count: capacity)
        var consumed = [Int32](repeating: 0, count: capacity)
        let total = Int(FuyiRimeQuery(raw, texts, &consumed, Int32(capacity)))
        defer { FuyiRimeFreeTexts(texts, Int32(total)); texts.deinitialize(count: capacity); texts.deallocate() }
        var result: [PinyinCandidate] = []
        var seen = Set<String>()
        for index in 0..<total {
            guard let pointer = texts[index], consumed[index] > 0, consumed[index] <= raw.count else { continue }
            let text = String(cString: pointer)
            // Do not translate Rime fallback candidates containing raw Latin input.
            guard !text.unicodeScalars.contains(where: { (65...90).contains($0.value) || (97...122).contains($0.value) }), seen.insert(text).inserted else { continue }
            result.append(PinyinCandidate(text: text, code: String(raw.prefix(Int(consumed[index]))), weight: 0))
        }
        lastRaw = raw; lastCandidates = result
        return result
    }
    func preview(_ raw: String) -> String {
        guard !raw.isEmpty else { return "" }
        return candidates(raw).first(where: { $0.code == raw })?.text ?? raw
    }
    func isComplete(_ raw: String) -> Bool { raw.isEmpty || candidates(raw).contains { $0.code == raw } }
    func clearActiveQuery() { lastRaw = nil; lastCandidates = []; FuyiRimeClear() }
}
