/// Recognizes a bare Shift tap without swallowing Shift+letter or shortcuts.
struct ShiftTapDetector {
    private var armedKey: Int?
    private var pressedAt: Double = 0
    private var suppressed = false
    mutating func noteKeyDown() {
        if armedKey != nil { armedKey = nil; suppressed = true }
    }
    mutating func reset() { armedKey = nil; suppressed = false; pressedAt = 0 }
    mutating func modifierChanged(keyCode: Int, shiftDown: Bool, otherModifiers: Bool, time: Double) -> Bool {
        if otherModifiers { armedKey = nil; suppressed = true }
        guard keyCode == 56 || keyCode == 60 else {
            if !shiftDown { reset() }
            return false
        }
        if shiftDown {
            guard !suppressed else { return false }
            if let armedKey, armedKey != keyCode { self.armedKey = nil; suppressed = true; return false }
            if armedKey == nil { armedKey = keyCode; pressedAt = time }
            return false
        }
        let toggles = !suppressed && armedKey == keyCode && time >= pressedAt && time - pressedAt < 1
        reset()
        return toggles
    }
}
