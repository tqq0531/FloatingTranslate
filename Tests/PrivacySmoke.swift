import AppKit
import Carbon

@main struct PrivacySmoke {
    @MainActor static func main() async {
        _ = NSApplication.shared
        let original = Preferences.shared.privacyMode
        defer { Preferences.shared.privacyMode = original }
        Preferences.shared.privacyMode = true
        do {
            _ = try await TranslationService.shared.english(for: "测试示例")
            fatalError("Privacy mode allowed translation")
        } catch PrivacyProtection.Blocked.secureOrPrivateInput {
            print("PASS privacy mode blocks translation before the framework call")
        } catch { fatalError("Unexpected privacy error type") }
        Preferences.shared.privacyMode = false
        let enabled = EnableSecureEventInput()
        guard enabled == noErr else { fatalError("Cannot enable secure input for test") }
        defer { DisableSecureEventInput() }
        guard PrivacyProtection.isBlocking else { fatalError("Secure input was not detected") }
        do {
            _ = try await TranslationService.shared.english(for: "测试示例")
            fatalError("Secure input allowed translation")
        } catch PrivacyProtection.Blocked.secureOrPrivateInput {
            print("PASS actual macOS Secure Input blocks translation")
        } catch { fatalError("Unexpected secure-input error type") }
        let bubble = FloatingPanel.shared
        bubble.model.source = "测试示例"; bubble.model.english = "Test example"; bubble.model.pinyin = "ceshi"
        bubble.hide()
        guard bubble.model.source.isEmpty && bubble.model.english.isEmpty && bubble.model.pinyin.isEmpty else { fatalError("Hidden bubble retained text") }
        print("PASS floating text cleared when hidden")
    }
}
