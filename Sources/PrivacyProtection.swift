import Foundation
import Carbon

extension Notification.Name {
    static let clearPrivateText = Notification.Name("FloatingTranslate.clearPrivateText")
}

@MainActor
enum PrivacyProtection {
    static var isBlocking: Bool {
        InputPrivacyPolicy.blocksProcessing(
            secureInput: IsSecureEventInputEnabled(),
            privacyMode: Preferences.shared.privacyMode
        )
    }
    enum Blocked: Error { case secureOrPrivateInput }
    static func requireAllowed() throws {
        guard !isBlocking else { throw Blocked.secureOrPrivateInput }
    }
}
