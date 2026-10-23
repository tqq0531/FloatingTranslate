import Foundation
import Security

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
    private let memory = CandidateMemory()
    private var lastMemoryEnabled = false
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
        let enabled = UserDefaults.standard.object(forKey: "rememberCandidates") as? Bool ?? true
        if lastRaw == raw && lastMemoryEnabled == enabled { return lastCandidates }
        let capacity = 100
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
        result = enabled ? memory.ranked(result, raw: raw) : result
        lastMemoryEnabled = enabled
        lastRaw = raw; lastCandidates = result
        return result
    }
    func remember(_ raw: String, candidate: PinyinCandidate) {
        guard UserDefaults.standard.object(forKey: "rememberCandidates") as? Bool ?? true else { return }
        memory.remember(raw: raw, candidate: candidate)
        lastRaw = nil
    }
    @discardableResult func clearMemory() -> Bool { let result = memory.clear(); clearActiveQuery(); return result }
    func preview(_ raw: String) -> String {
        guard !raw.isEmpty else { return "" }
        return candidates(raw).first(where: { $0.code == raw })?.text ?? raw
    }
    func isComplete(_ raw: String) -> Bool { raw.isEmpty || candidates(raw).contains { $0.code == raw } }
    func clearActiveQuery() { lastRaw = nil; lastCandidates = []; FuyiRimeClear() }
}

/// Only explicitly selected short Chinese candidates, not prose or translations.
/// The OS login Keychain encrypts the bounded data. A locked/unavailable
/// Keychain fails without an authentication prompt or blocking input.
final class CandidateMemory {
    private var counts: [String: Int] = [:]
    private let persistent: Bool
    private var query: [String: Any] { [kSecClass as String: kSecClassGenericPassword,
        kSecAttrService as String: "com.fuyi.inputmethod.CandidateCounts",
        kSecAttrAccount as String: "short-candidates-v1",
        kSecUseAuthenticationUI as String: kSecUseAuthenticationUIFail] }
    init(persistent: Bool = true) {
        self.persistent = persistent
        guard persistent else { return }
        var q = query; q[kSecReturnData as String] = true
        var result: CFTypeRef?
        if SecItemCopyMatching(q as CFDictionary, &result) == errSecSuccess,
           let data = result as? Data, data.count < 300_000,
           let saved = try? JSONDecoder().decode([String: Int].self, from: data), saved.count <= 2048 {
            counts = saved.filter { $0.value > 0 && $0.value <= 1_000_000 }
        }
    }
    private func key(_ raw: String, _ candidate: PinyinCandidate) -> String { raw + "\t" + candidate.text }
    func remember(raw: String, candidate: PinyinCandidate) {
        guard !raw.isEmpty, raw.count <= 100,
              raw.unicodeScalars.allSatisfy({ (97...122).contains($0.value) || $0.value == 39 }),
              !candidate.text.isEmpty, candidate.text.count <= 4,
              candidate.text.unicodeScalars.allSatisfy({ (0x3400...0x9fff).contains($0.value) || (0x20000...0x3134f).contains($0.value) }) else { return }
        let k = key(raw, candidate)
        guard counts[k] != nil || counts.count < 2048 else { return }
        counts[k] = min(1_000_000, (counts[k] ?? 0) + 1)
        save()
    }
    func ranked(_ candidates: [PinyinCandidate], raw: String) -> [PinyinCandidate] {
        // Preserve complete candidates before partial prefixes, and never move a
        // short memorized word ahead of a complete long phrase or sentence.
        guard candidates.contains(where: { (counts[key(raw, $0)] ?? 0) > 0 }),
              !candidates.contains(where: { $0.code == raw && $0.text.count > 4 }) else { return candidates }
        return candidates.enumerated().sorted {
            let a = $0.element, b = $1.element
            if (a.code == raw) != (b.code == raw) { return a.code == raw }
            let ac = counts[key(raw, a)] ?? 0, bc = counts[key(raw, b)] ?? 0
            return ac == bc ? $0.offset < $1.offset : ac > bc
        }.map { $0.element }
    }
    @discardableResult func clear() -> Bool {
        counts = [:]
        guard persistent else { return true }
        let status = SecItemDelete(query as CFDictionary)
        return status == errSecSuccess || status == errSecItemNotFound
    }
    private func save() {
        guard persistent, let data = try? JSONEncoder().encode(counts) else { return }
        let attributes = [kSecValueData as String: data]
        let status = SecItemUpdate(query as CFDictionary, attributes as CFDictionary)
        if status == errSecItemNotFound {
            var q = query; q[kSecValueData as String] = data
            q[kSecAttrAccessible as String] = kSecAttrAccessibleWhenUnlockedThisDeviceOnly
            SecItemAdd(q as CFDictionary, nil)
        }
    }
}
