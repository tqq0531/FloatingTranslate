import AppKit
import SwiftUI
import Translation

struct SettingsView: View {
    @ObservedObject var installation = InstallationManager.shared
    @State private var showPrivacy = false
    @ObservedObject var preferences = Preferences.shared
    @State private var text = ""
    @State private var translated = ""
    @State private var status = "输入中文，停顿片刻即可预览英文翻译"
    @State private var downloadStatus = "使用 Apple 本机翻译 · 需要 macOS 26 或更新版本"
    @State private var configuration: TranslationSession.Configuration?
    @State private var preparing = false
    @State private var copied = false
    @State private var memoryStatus = ""
    private let ink = Color(red: 0.12, green: 0.22, blue: 0.25)
    @MainActor private var previewModel: BubbleModel {
        let model = BubbleModel()
        model.toChinese = preferences.directEnglish
        if text.isEmpty {
            model.source = "你好吗"
            model.english = "How are you?"
            model.pinyin = "ni hao ma"
            model.candidates = ["你好吗", "你号码", "你好", "你", "尼"].map {
                PinyinCandidate(text: $0, code: "", weight: 0)
            }
            if preferences.directEnglish {
                model.source = "How are you?"; model.english = "你好吗？"; model.pinyin = ""; model.candidates = []
            }
        } else {
            model.source = text
            model.english = translated
            model.status = status
        }
        return model
    }
    var body: some View {
        HStack(spacing: 0) {
            VStack(alignment: .leading, spacing: 24) {
                HStack(spacing: 10) {
                    Image(systemName: "character.bubble.fill").font(.system(size: 28)).foregroundStyle(.teal)
                    Text("浮译").font(.system(size: 26, weight: .bold))
                }
                Text("想到中文\n表达世界。")
                    .font(.system(size: 37, weight: .semibold)).lineSpacing(8)
                Text("打字的同时，让英文自然浮现。")
                    .font(.system(size: 14)).foregroundStyle(.secondary)
                Spacer()
                Label("本机翻译", systemImage: "lock.shield")
                Text("翻译正文在设备上处理。\n首次准备语言包时需要联网。")
                    .font(.system(size: 12)).foregroundStyle(.secondary).lineSpacing(5)
                Button("隐私与密码输入说明") { showPrivacy = true }
                    .buttonStyle(.link).font(.system(size: 12))
                Text("MAC 本地试用版 · 0.5.13").font(.system(size: 10, weight: .medium, design: .monospaced)).foregroundStyle(.secondary)
            }
            .padding(32).frame(width: 290).frame(maxHeight: .infinity)
            .background(Color(red: 0.89, green: 0.95, blue: 0.93))
            ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Text("输入与翻译").font(.system(size: 24, weight: .semibold))
                    Spacer()
                    Text("中文 ⇄ English").font(.system(size: 12)).foregroundStyle(.secondary)
                }
                Picker("输入模式", selection: $preferences.directEnglish) {
                    Text("中文拼音").tag(false)
                    Text("English 英译中").tag(true)
                }.pickerStyle(.segmented).labelsHidden()
                Text("单独按 Shift 切换中英文；Shift 加字母仍用于大写。")
                    .font(.system(size: 11)).foregroundStyle(.secondary)
                Text("拼音转换后默认输出英文").font(.system(size: 13, weight: .medium))
                Picker("中文模式的默认输出", selection: $preferences.englishDefault) {
                    Text("中文 · 回车输入原文").tag(false)
                    Text("English · 回车输入译文").tag(true)
                }.pickerStyle(.segmented).labelsHidden()
                Toggle("英文转换后默认输出中文", isOn: $preferences.chineseDefault)
                    .font(.system(size: 13)).toggleStyle(.switch)
                Text("英文模式：回车提交默认语言，Tab 提交另一语言；空格继续当前句子。")
                    .font(.system(size: 11)).foregroundStyle(.secondary)
                HStack {
                    Toggle("记住常选短词", isOn: $preferences.rememberCandidates).toggleStyle(.switch)
                    Button("清除选词记忆") { memoryStatus = InputSession.engine.clearMemory() ? "选词记忆已清除" : "内存已清除；请解锁钥匙串后重试删除保存记录" }
                }.font(.system(size: 12))
                if !memoryStatus.isEmpty { Text(memoryStatus).font(.system(size: 11)).foregroundStyle(.secondary) }
                Text("常选短词计数保存在本机钥匙串；= 下一页，- 上一页。")
                    .font(.system(size: 11)).foregroundStyle(.secondary)
                Text("逐词输入也翻译整句；支持 nh 简拼与常见拼音纠错。")
                    .font(.system(size: 11)).foregroundStyle(.secondary)
                Toggle("隐私模式：直接输入英文，暂停拼音和翻译", isOn: $preferences.privacyMode)
                    .font(.system(size: 12)).toggleStyle(.switch)
                Text("英文与候选词共用一个浮窗，随输入文字位置显示。")
                    .font(.system(size: 11)).foregroundStyle(.secondary)
                VStack(alignment: .leading, spacing: 4) {
                    HStack {
                        Text(text.isEmpty ? "翻译示例 · 英文与候选词一起显示" : "实时翻译预览")
                            .font(.system(size: 11)).foregroundStyle(.secondary)
                        Spacer()
                        if !translated.isEmpty {
                            Button(copied ? "已复制" : "复制译文") {
                                NSPasteboard.general.clearContents()
                                NSPasteboard.general.setString(translated, forType: .string)
                                copied = true
                            }.buttonStyle(.borderless).font(.system(size: 11))
                        }
                    }
                    TranslationBubble(model: previewModel, width: 474)
                }
                VStack(alignment: .leading, spacing: 8) {
                    Text(preferences.directEnglish ? "试着输入英文" : "试着输入中文").font(.system(size: 11)).foregroundStyle(.secondary)
                    TextEditor(text: $text).font(.system(size: 16)).scrollContentBackground(.hidden)
                        .frame(height: 90)
                    Text("例如：你好，很高兴认识你。")
                        .font(.system(size: 11)).foregroundStyle(.tertiary)
                }.padding(14).background(.white, in: RoundedRectangle(cornerRadius: 12))
                HStack(alignment: .top) {
                    VStack(alignment: .leading, spacing: 5) {
                        Text("准备翻译语言包").font(.system(size: 12, weight: .medium))
                        Text(downloadStatus).font(.system(size: 11)).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                    Spacer()
                    Button(preparing ? "准备中…" : "下载 / 检查") {
                        preparing = true
                        configuration?.invalidate()
                        configuration = TranslationSession.Configuration(source: Locale.Language(identifier: preferences.directEnglish ? "en" : "zh-Hans"), target: Locale.Language(identifier: preferences.directEnglish ? "zh-Hans" : "en"))
                    }.disabled(preparing)
                }
                Divider()
                VStack(alignment: .leading, spacing: 7) {
                    HStack {
                        Text("安装与修复").font(.system(size: 12, weight: .medium))
                        Spacer()
                        Button("启用并试用") { AppDelegate.shared.openInputSetup() }
                            .disabled(installation.busy || !installation.isInstalled)
                        Button(installation.busy ? "处理中…" : "安装 / 修复输入法") { installation.installOrRepair() }
                            .disabled(installation.busy)
                    }
                    Text(installation.status).font(.system(size: 11)).foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }.padding(30)
            }.frame(width: 550)
        }
        .foregroundStyle(ink)
        .background(Color(red: 0.96, green: 0.97, blue: 0.96))
        .frame(width: 840, height: 780)
        .alert("隐私与密码输入", isPresented: $showPrivacy) {
            Button("知道了", role: .cancel) { }
        } message: {
            Text("正文使用 Apple 本机翻译，不上传至开发者或第三方翻译服务。首次下载语言包需要联网；Apple 可能收集不含正文的使用与性能指标。浮译不保存连续正文或译文历史，不读取剪贴板。可选的短词选词计数保存在本机钥匙串，可关闭或清除。检测到系统安全输入时停止处理并清空浮窗。自定义密码框可能未启用安全输入，因此输入密码请切换到系统 ABC，不要在试用框输入密码。")
        }
        .onReceive(NotificationCenter.default.publisher(for: .clearPrivateText)) { _ in
            text = ""; translated = ""; copied = false
            status = "输入中文，停顿片刻即可预览英文翻译"
        }
        .task(id: PreviewRequest(text: text, privacyMode: preferences.privacyMode, toChinese: preferences.directEnglish)) {
            translated = ""; copied = false
            let source = text.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !source.isEmpty else { status = "输入中文，停顿片刻即可预览英文翻译"; return }
            guard !PrivacyProtection.isBlocking else { status = "隐私模式或系统安全输入已开启，暂停翻译"; return }
            status = "正在翻译…"
            do {
                try await Task.sleep(for: .milliseconds(450))
                let result = try await TranslationService.shared.translate(source, toChinese: preferences.directEnglish)
                guard !Task.isCancelled else { return }
                translated = result
            } catch {
                guard !Task.isCancelled else { return }
                if PrivacyProtection.isBlocking { status = "已暂停翻译"; return }
                status = "翻译暂不可用，请先下载 / 检查中英语言包。"
            }
        }
        .translationTask(configuration) { session in
            do {
                try await session.prepareTranslation()
                let availability = LanguageAvailability()
                let forward = await availability.status(from: Locale.Language(identifier: "zh-Hans"), to: Locale.Language(identifier: "en"))
                let reverse = await availability.status(from: Locale.Language(identifier: "en"), to: Locale.Language(identifier: "zh-Hans"))
                downloadStatus = "中译英：\(forward == .installed ? "已就绪" : "待准备") · 英译中：\(reverse == .installed ? "已就绪" : "待准备")"
            } catch { downloadStatus = "语言包准备未完成：\(error.localizedDescription)" }
            preparing = false
        }
    }
}

private struct PreviewRequest: Equatable {
    let text: String
    let privacyMode: Bool
    let toChinese: Bool
}
