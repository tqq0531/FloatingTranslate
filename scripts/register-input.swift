import Foundation
import Carbon

private let fuyiID = "com.fuyi.inputmethod.FloatingTranslate"
private let modeID = "com.fuyi.inputmethod.FloatingTranslate.Hans"
func preferenceStore(modernExists: Bool, systemMajor: Int) -> (domain: String, key: String) {
    (modernExists || systemMajor >= 26) ? ("com.apple.inputsources", "AppleEnabledThirdPartyInputSources") : ("com.apple.HIToolbox", "AppleEnabledInputSources")
}
private let modernExists = CFPreferencesCopyValue("AppleEnabledThirdPartyInputSources" as CFString, "com.apple.inputsources" as CFString, kCFPreferencesCurrentUser, kCFPreferencesAnyHost) != nil
private let store = preferenceStore(modernExists: modernExists, systemMajor: ProcessInfo.processInfo.operatingSystemVersion.majorVersion)
private let domain = store.domain as CFString
private let enabledKey = store.key as CFString

func fail(_ message: String, _ code: Int32 = 1) -> Never {
    fputs(message + "\n", stderr); exit(code)
}
// Preserve every existing entry and its order. Only append our missing identities.
func repairedEntries(_ entries: [[String: Any]]) -> [[String: Any]] {
    var result = entries
    let required = [
        ["Bundle ID": fuyiID, "InputSourceKind": "Keyboard Input Method"],
        ["Bundle ID": fuyiID, "InputSourceKind": "Input Mode", "Input Mode": modeID]
    ]
    for identity in required where !result.contains(where: { entry in
        identity.allSatisfy { entry[$0.key] as? String == $0.value }
    }) { result.append(identity) }
    return result
}
func currentEntries() throws -> [[String: Any]] {
    guard let raw = CFPreferencesCopyValue(enabledKey, domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost) else { return [] }
    guard let entries = raw as? [[String: Any]] else {
        throw NSError(domain: "FuyiInstall", code: 1, userInfo: [NSLocalizedDescriptionKey: "系统输入源清单格式未知，未修改。"])
    }
    return entries
}
func hasPersistentMode(_ entries: [[String: Any]]) -> Bool {
    entries.contains { $0["Bundle ID"] as? String == fuyiID && $0["Input Mode"] as? String == modeID && $0["InputSourceKind"] as? String == "Input Mode" }
    && entries.contains { $0["Bundle ID"] as? String == fuyiID && $0["InputSourceKind"] as? String == "Keyboard Input Method" }
}
func repairMenu() throws {
    let entries = try currentEntries()
    let updated = repairedEntries(entries)
    guard !NSArray(array: entries).isEqual(to: updated) else {
        print("MENU: 浮译的持久启用登记完整"); return
    }
    let folder = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/FloatingTranslate/InstallBackups")
    try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
    let backup = folder.appendingPathComponent("InputSources-\(UUID().uuidString).plist")
    let data = try PropertyListSerialization.data(fromPropertyList: ["PreferencesDomain": store.domain, "PreferencesKey": store.key, "Entries": entries], format: .xml, options: 0)
    try data.write(to: backup, options: .atomic)
    CFPreferencesSetValue(enabledKey, updated as CFArray, domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost)
    guard CFPreferencesSynchronize(domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost), hasPersistentMode(try currentEntries()) else {
        throw NSError(domain: "FuyiInstall", code: 2, userInfo: [NSLocalizedDescriptionKey: "输入源清单未成功同步，请在系统设置中重新添加浮译。备份：\(backup.path)"])
    }
    print("BACKUP: \(backup.path)")
    print("MENU: 已补齐浮译的输入法与中文模式登记")
    // Refresh only the menu agent. Never select a source or restart a browser.
    let refresh = Process(); refresh.executableURL = URL(fileURLWithPath: "/usr/bin/killall")
    refresh.arguments = ["TextInputMenuAgent"]
    refresh.standardOutput = FileHandle.nullDevice; refresh.standardError = FileHandle.nullDevice
    try refresh.run(); refresh.waitUntilExit()
}
func stringProperty(_ source: TISInputSource, _ key: CFString) -> String {
    guard let pointer = TISGetInputSourceProperty(source, key) else { return "" }
    return Unmanaged<CFString>.fromOpaque(pointer).takeUnretainedValue() as String
}
func boolProperty(_ source: TISInputSource, _ key: CFString) -> Bool {
    guard let pointer = TISGetInputSourceProperty(source, key) else { return false }
    return CFBooleanGetValue(Unmanaged<CFBoolean>.fromOpaque(pointer).takeUnretainedValue())
}
if CommandLine.arguments.contains("--self-test") {
    precondition(preferenceStore(modernExists: true, systemMajor: 26).domain == "com.apple.inputsources")
    precondition(preferenceStore(modernExists: false, systemMajor: 15).domain == "com.apple.HIToolbox")
    precondition(preferenceStore(modernExists: false, systemMajor: 26).domain == "com.apple.inputsources")
    let original: [[String: Any]] = [["InputSourceKind": "Keyboard Layout", "KeyboardLayout Name": "ABC", "KeyboardLayout ID": 252], ["Bundle ID": "other.ime", "InputSourceKind": "Input Mode", "Input Mode": "other.ime.custom", "extra": ["keep": true]]]
    let repaired = repairedEntries(original)
    precondition(NSArray(array: Array(repaired.prefix(original.count))).isEqual(to: original))
    precondition(repaired.count == original.count + 2 && hasPersistentMode(repaired))
    precondition(NSArray(array: repairedEntries(repaired)).isEqual(to: repaired))
    let onlyParent = Array(repaired.dropLast())
    precondition(repairedEntries(onlyParent).count == repaired.count)
    precondition(hasPersistentMode(repairedEntries([])))
    print("PASS preserve other input sources, append both identities, repair missing mode, idempotence, empty list")
    exit(0)
}
guard CommandLine.arguments.count >= 2 else { fail("Usage: RegisterInputSource /path/to/input.app [--check | --repair-menu]") }
let url = URL(fileURLWithPath: CommandLine.arguments[1]).standardizedFileURL
// Repairs are restricted to the installed Fuyi bundle, not arbitrary source identities.
guard let bundle = Bundle(url: url), bundle.bundleIdentifier == fuyiID else { fail("输入法应用不存在或标识无效") }
if CommandLine.arguments.contains("--repair-menu") {
    let installed = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Input Methods/浮译.app").resolvingSymlinksInPath()
    guard url.resolvingSymlinksInPath() == installed else { fail("请先安装浮译，再修复输入法菜单。") }
}
if !CommandLine.arguments.contains("--check") {
    let status = TISRegisterInputSource(url as CFURL)
    guard status == noErr else { fail("输入法注册失败，系统错误码：\(status)") }
    print("REGISTERED: \(fuyiID)")
}
let filter = [kTISPropertyBundleID as String: fuyiID] as CFDictionary
guard let list = TISCreateInputSourceList(filter, true) else { fail("系统尚未返回输入法列表。", 2) }
let sources = list.takeRetainedValue() as! [TISInputSource]
guard sources.contains(where: { stringProperty($0, kTISPropertyInputSourceID) == modeID && boolProperty($0, kTISPropertyInputSourceIsSelectCapable) }) else { fail("系统尚未识别浮译中文模式，请重新登录后再试。", 2) }
if CommandLine.arguments.contains("--repair-menu") {
    do { try repairMenu() } catch { fail(error.localizedDescription) }
}
for source in sources {
    print("SOURCE: \(stringProperty(source, kTISPropertyLocalizedName)) | \(stringProperty(source, kTISPropertyInputSourceID)) | enabled=\(boolProperty(source, kTISPropertyInputSourceIsEnabled)) | selectable=\(boolProperty(source, kTISPropertyInputSourceIsSelectCapable))")
}
do {
    let persisted = hasPersistentMode(try currentEntries())
    print("MENU_STORE: \(store.domain).\(store.key)")
    print("MENU_PERSISTED: \(persisted)")
    if CommandLine.arguments.contains("--repair-menu") && !persisted { fail("菜单登记仍不完整，未确认修复成功。", 2) }
} catch { fail(error.localizedDescription) }
