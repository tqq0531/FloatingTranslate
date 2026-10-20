import AppKit
import Combine

@MainActor
final class InstallationManager: ObservableObject {
    static let shared = InstallationManager()
    @Published var busy = false
    @Published var status = "点击安装 / 修复，将安装并补齐「浮译」的切换菜单登记。"
    static let identifier = "com.fuyi.inputmethod.FloatingTranslate"
    private let files = FileManager.default
    var destination: URL {
        files.homeDirectoryForCurrentUser.appendingPathComponent("Library/Input Methods/浮译.app")
    }
    var isInstalled: Bool {
        Bundle(url: destination)?.bundleIdentifier == Self.identifier
    }
    enum InstallError: LocalizedError {
        case failed(String)
        var errorDescription: String? {
            switch self { case .failed(let message): return message }
        }
    }
    private func run(_ executable: URL, _ arguments: [String]) throws -> String {
        let process = Process()
        process.executableURL = executable
        process.arguments = arguments
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        try process.run()
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        let output = String(data: data, encoding: .utf8) ?? ""
        guard process.terminationStatus == 0 else {
            throw InstallError.failed("安装检查未通过：\(output.prefix(1500))")
        }
        return output
    }
    @discardableResult
    func installOrRepair(openSettings: Bool = true) -> Bool {
        guard !busy else { return false }
        busy = true
        defer { busy = false }
        do {
            let source = Bundle.main.bundleURL.standardizedFileURL.resolvingSymlinksInPath()
            let target = destination.standardizedFileURL.resolvingSymlinksInPath()
            guard Bundle(url: source)?.bundleIdentifier == Self.identifier else {
                throw InstallError.failed("应用标识无效，请从项目发布页重新下载。")
            }
            if source != target {
                try files.createDirectory(at: target.deletingLastPathComponent(), withIntermediateDirectories: true)
                let stage = target.deletingLastPathComponent().appendingPathComponent(".fuyi-install-\(UUID().uuidString)")
                defer { try? files.removeItem(at: stage) }
                try files.createDirectory(at: stage, withIntermediateDirectories: true)
                let stagedApp = stage.appendingPathComponent("浮译.app")
                _ = try run(URL(fileURLWithPath: "/usr/bin/ditto"), ["--noextattr", "--norsrc", source.path, stagedApp.path])
                // Keep quarantine: do not silently bypass macOS download protection.
                for attribute in ["com.apple.quarantine"] {
                    let read = Process(); let pipe = Pipe()
                    read.executableURL = URL(fileURLWithPath: "/usr/bin/xattr")
                    read.arguments = ["-p", attribute, source.path]
                    read.standardOutput = pipe; read.standardError = FileHandle.nullDevice
                    try read.run()
                    let data = pipe.fileHandleForReading.readDataToEndOfFile(); read.waitUntilExit()
                    if read.terminationStatus == 0, let value = String(data: data, encoding: .utf8)?.trimmingCharacters(in: .newlines) {
                        _ = try run(URL(fileURLWithPath: "/usr/bin/xattr"), ["-w", attribute, value, stagedApp.path])
                    }
                }
                _ = try run(URL(fileURLWithPath: "/usr/bin/codesign"), ["--verify", "--deep", "--strict", stagedApp.path])
                var backup: URL?
                if files.fileExists(atPath: target.path) {
                    guard Bundle(url: target)?.bundleIdentifier == Self.identifier else {
                        throw InstallError.failed("目标位置存在其他应用，未替换。")
                    }
                    let folder = files.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/FloatingTranslate/Backups/\(UUID().uuidString)")
                    try files.createDirectory(at: folder, withIntermediateDirectories: true)
                    let old = folder.appendingPathComponent("浮译.app.backup")
                    try files.moveItem(at: target, to: old); backup = old
                }
                do { try files.moveItem(at: stagedApp, to: target) }
                catch {
                    if let backup { try? files.moveItem(at: backup, to: target) }
                    throw error
                }
            }
            _ = try run(URL(fileURLWithPath: "/usr/bin/codesign"), ["--verify", "--deep", "--strict", target.path])
            let helper = target.appendingPathComponent("Contents/MacOS/RegisterInputSource")
            _ = try run(helper, [target.path, "--repair-menu"])
            _ = try run(helper, [target.path, "--check"])
            status = "已安装并核实系统登记。点击“启用并试用”，输入 nihao 并按空格；确认出现中文后，再从菜单栏切换使用。"
            if openSettings { AppDelegate.shared.openInputSetup() }
            return true
        } catch { status = error.localizedDescription; return false }
    }
    func openKeyboardSettings() {
        if let url = URL(string: "x-apple.systempreferences:com.apple.Keyboard-Settings.extension") { NSWorkspace.shared.open(url) }
    }
}
