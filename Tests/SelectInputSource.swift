import Foundation
import Carbon
func sourceID(_ source: TISInputSource) -> String {
    guard let pointer = TISGetInputSourceProperty(source, kTISPropertyInputSourceID) else { return "" }
    return Unmanaged<CFString>.fromOpaque(pointer).takeUnretainedValue() as String
}
if CommandLine.arguments.count == 1 {
    print(sourceID(TISCopyCurrentKeyboardInputSource().takeRetainedValue()))
} else {
    let filter = [kTISPropertyInputSourceID as String: CommandLine.arguments[1]] as CFDictionary
    guard let result = TISCreateInputSourceList(filter, false),
          let source = (result.takeRetainedValue() as! [TISInputSource]).first else { exit(2) }
    let status = TISSelectInputSource(source)
    print("Selected input source, status=\(status)")
    exit(status == noErr ? 0 : 1)
}
