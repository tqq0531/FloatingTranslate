import Foundation
import Translation
@main struct Smoke {
    static func main() async {
        let source = Locale.Language(identifier: "zh-Hans")
        let target = Locale.Language(identifier: "en")
        let status = await LanguageAvailability().status(from: source, to: target)
        guard status == .installed else {
            print("LANGUAGE_PACK_MISSING: download the Chinese/English pair in the app before translation can be verified.")
            return
        }
        do {
            let result = try await TranslationSession(installedSource: source, target: target).translate("我今天去北京。")
            guard !result.targetText.isEmpty, result.targetText != result.sourceText else { fatalError("Translation did not produce English") }
            print("TRANSLATION_OK: \(result.targetText)")
        } catch { print("TRANSLATION_FAILED: \(error)"); exit(1) }
    }
}
