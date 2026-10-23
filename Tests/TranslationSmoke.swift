import Foundation
import Translation
@main struct Smoke {
    static func main() async {
        for (from, to, input) in [("zh-Hans", "en", "我今天去北京。"), ("en", "zh-Hans", "Have you worked out today?")] {
            let source = Locale.Language(identifier: from), target = Locale.Language(identifier: to)
            guard await LanguageAvailability().status(from: source, to: target) == .installed else {
                print("LANGUAGE_PACK_MISSING: \(from) -> \(to); prepare this direction in Settings.")
                continue
            }
            do {
                let result = try await TranslationSession(installedSource: source, target: target).translate(input)
                guard !result.targetText.isEmpty, result.targetText != result.sourceText else { fatalError("Translation produced no target text") }
                print("TRANSLATION_OK \(from) -> \(to): \(result.targetText)")
            } catch { print("TRANSLATION_FAILED: \(error)"); exit(1) }
        }
    }
}
