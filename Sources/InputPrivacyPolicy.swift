/// Pure policy so secure-input behavior can be verified without typing a password.
enum InputPrivacyPolicy {
    static func blocksProcessing(secureInput: Bool, privacyMode: Bool) -> Bool {
        secureInput || privacyMode
    }
}

