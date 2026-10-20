import Foundation
import Translation

@MainActor
final class TranslationService {
    static let shared = TranslationService()
    // No input history, disk writes, or translation cache.
    func english(for source: String) async throws -> String {
        try PrivacyProtection.requireAllowed()
        try Task.checkCancellation()
        let session = TranslationSession(installedSource: Locale.Language(identifier: "zh-Hans"), target: Locale.Language(identifier: "en"))
        let result = try await session.translate(source)
        try Task.checkCancellation()
        try PrivacyProtection.requireAllowed()
        return result.targetText
    }
}

@MainActor
final class Preferences: ObservableObject {
    static let shared = Preferences()
    @Published var directEnglish: Bool {
        didSet { UserDefaults.standard.set(directEnglish, forKey: "directEnglish") }
    }
    @Published var englishDefault: Bool {
        didSet { UserDefaults.standard.set(englishDefault, forKey: "englishDefault") }
    }
    @Published var translationAbove: Bool {
        didSet { UserDefaults.standard.set(translationAbove, forKey: "translationAbove") }
    }
    @Published var privacyMode: Bool {
        didSet {
            UserDefaults.standard.set(privacyMode, forKey: "privacyMode")
            if privacyMode {
                FloatingPanel.shared.hide()
                NotificationCenter.default.post(name: .clearPrivateText, object: nil)
            }
        }
    }
    init() {
        directEnglish = UserDefaults.standard.bool(forKey: "directEnglish")
        privacyMode = UserDefaults.standard.bool(forKey: "privacyMode")
        translationAbove = UserDefaults.standard.object(forKey: "translationAbove") as? Bool ?? true
        englishDefault = UserDefaults.standard.bool(forKey: "englishDefault") }
}
