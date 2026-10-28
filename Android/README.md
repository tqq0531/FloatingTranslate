> Android 0.1.8：长译文和拼音支持左右滑动；本次拼音最多 80 个字母，达到上限提示先选词上屏。新增加密选词记忆与联想、独立数字/符号入口和圆形工具按钮。/ Scrollable translation and Pinyin, an explicit 80-letter composition limit, encrypted local learning, and revised keyboard controls.

> Android 0.1.7：新增可切换的雾凇常用词扩展、单字简拼与混合简拼，改进键盘稳定布局、键帽与按压反馈。[详情与词库许可](docs/keyboard-017.md)。两种词库均离线运行。

# 浮译 Android 0.1.8 / FloatingTranslate for Android

轻量中英翻译键盘：中文拼音、英文输入、离线双向翻译，以及保留 24 小时的文字剪贴板。0.1.6 重新设计按键和布局，提供自定义高度及跟随系统的深浅色外观。

A focused bilingual keyboard with Pinyin, English typing, fully offline translation in both directions, and 24-hour text clipboard history. Version 0.1.6 redesigns the keyboard with adjustable height and system-aware light/dark appearance.

## 安装与使用 / Install and use

1. 安装完整 APK，打开“浮译”，点击“启用键盘”，再选择浮译。试用包使用开发签名。
2. 底部“拼 / 英”或顶部“中 → 英 / 英 → 中”切换输入语言及翻译方向。
3. 中文输入拼音后点候选或空格确认；英文空格会保留在同一句中，继续输入会重新翻译完整句子。
4. 点“用英文 / 用中文”将本次输入替换为译文，长按译文可复制。翻译未就绪时按钮不可用。操作键遵循宿主显示“发送、搜索、完成、换行”；正在组词时先确认，之后再执行发送等动作。
5. 点击键盘滑杆图标（调节），选择“跟随系统 / 浅色 / 深色”，拖动滑块改变高度。标准为 100%，约可调至 81%–126%，设置会保留；也可从应用设置调整。
6. 开启“复制历史”，点键盘剪贴板图标查看并插入以前复制的文字；记录保留 24 小时，可逐条删除或清空。

Install the complete APK and enable/select FloatingTranslate in Android keyboard settings. The language key also changes translation direction. English spaces remain part of the current sentence. “用英文 / 用中文” replaces the current draft with its translation; long-press to copy. The action key confirms an active composition before sending/searching/inserting a line break according to the host app. “调节” provides persistent height and light/dark/system choices. “历史” opens clipboard entries for insertion or deletion.

设置中的“中文输入，默认输出英文”控制拼音确认时的输出；“英文输入，默认输出中文”控制英文回车确认时的输出。英文空格始终用于继续写句子。替换只针对本次键盘维护、且与光标前原文完全一致的草稿；光标移动或宿主不提供该范围时不会删除正文，可长按复制译文。

The two independent output defaults apply when confirming Chinese or English composition. English spaces keep extending the sentence. Replacement is limited to the exact current draft immediately before the cursor; if the editor cannot verify that range, use copy instead.

## 新外观 / Appearance

白键浅灰底与石墨深色两套配色，用蓝紫色突出翻译和操作键。标准错位 QWERTY、宽空格、独立中英键，保留数字和符号；移除表情工具等额外入口。按键间隙仍属于触控范围，退格长按支持连续删除。Shift 只切换下一个字母的大写。

Neutral white keys and graphite dark surfaces use a restrained blue-violet accent. Staggered QWERTY rows, a wide space bar and a dedicated language key preserve familiar typing geometry. Numbers and symbols remain; extra emoji tools have been removed. Touch gutters remain active, Backspace repeats while held, and Shift capitalizes the next character.

[浅色键盘 / Light](docs/keyboard-light.png) · [深色键盘 / Dark](docs/keyboard-dark.png) · [高度和主题 / Appearance controls](docs/keyboard-adjust-dark.png) · [英译中 / English to Chinese](docs/keyboard-english-chinese.png)

## 离线翻译 / Offline translation

中译英与英译中模型均随 APK 内置，手机无需联网、Google 服务或下载语言包。首次启动从 APK 解压并校验两个方向，设置页显示真实进度。模型文件总计约 232 MB，安装和提取需预留空间（建议至少 1 GB）。推理只保留当前方向的模型会话，切换方向需要重新加载。

Both models are bundled in the APK. First launch extracts and verifies them with progress; no network or Google services are required on the phone. Model assets total approximately 232 MB; allow at least 1 GB of free storage for installation and extraction. Only one direction is kept resident at a time.

OPUS-MT int8 模型通过 ONNX Runtime 在 CPU 上推理，使用 beam search 和重复 token 抑制。输入最多 256 个模型子词，输出最多 128 个子词，超过时提示分句；翻译仍可能出错，不提供语法纠正保证。运行时 APK 没有 `INTERNET` 权限，不调用云接口。

Quantized OPUS-MT runs on the CPU with beam decoding and repeated-token suppression. The model limit is 256 input subwords and 128 output subwords. Translation can be inaccurate; this is not a guaranteed grammar checker. The APK has no `INTERNET` permission or cloud fallback.

## 历史与隐私 / History and privacy

- 历史默认关闭，开启后仅收集文字，最多 100 条，每条最多 10,000 个 UTF-16 字符。不导入图片、文件或 URI。
- 每条从复制时间起保存 24 小时；重复复制不延长已有记录期限。过期记录不可见、不可插入；休眠或关机可能推迟磁盘清理。
- 使用 Android Keystore 和 AES-GCM 加密，存于应用私有 no-backup 目录。关闭开关即清空；清空浮译不会改动系统当前剪贴板。
- 仅在系统允许默认输入法读取剪贴板时收集；切换其他输入法或服务被停止期间无法保证记录。
- 密码框、不允许个性化学习的字段与带敏感标记的剪贴板会跳过。未正确标记的自定义敏感字段可能无法识别。
- 输入正文、译文不写日志、不上传，不保存打字历史。复制历史需显式开启；默认开启的“选词记忆与联想”另存最多 2000 条拼音、选词及相邻选词关系，使用 Keystore/AES-GCM 加密。可在设置关闭或清空，不保存整段输入正文。

Clipboard history is opt-in, text-only, encrypted locally, excluded from backup, and capped at 100 entries. Entries expire 24 hours after their original copy time; copying duplicates does not renew them. Expired entries cannot be inserted even if device sleep delays file cleanup. Disabling history clears it. Android may restrict clipboard access when this keyboard is not active. Password/sensitive fields and marked sensitive clips are excluded where detectable. Typed text and translations are neither logged nor uploaded. Local learning is enabled by default and stores up to 2,000 encrypted spelling/word and adjacent-word records, separately from clipboard history; it can be disabled or cleared in settings.

## 开发与验证 / Development and verification

需要 JDK 17/21、Android SDK 36、Build Tools 36.1.0。开发机首次获取依赖及模型需要网络，安装完整 APK 的手机不需要。两个方向的权重不提交到 Git；准备脚本固定上游版本，构建会检查全部文件大小与 SHA-256。

Development requires JDK 17/21 and Android SDK 36. Initial dependency/model preparation needs network access on the build machine only. Large weights are excluded from Git, pinned by the preparation script, and verified by Gradle before packaging.

```sh
cd Android
python3 -m venv .model-venv
.model-venv/bin/pip install -r tools/requirements-translation.txt
.model-venv/bin/python tools/prepare_translation.py
./gradlew :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
# Run on a dedicated emulator/test device after selecting FloatingTranslate:
./gradlew :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.fuyi.keyboard.KeyboardDeviceTest,com.fuyi.keyboard.SettingsDeviceTest,com.fuyi.keyboard.ClipboardDeviceTest \
  -Pandroid.testInstrumentationRunnerArguments.preparedModel=true
# Run model validation separately from UI tests that initialize background sessions:
./gradlew :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.fuyi.keyboard.TranslationModelDeviceTest
```

产物：`app/build/outputs/apk/debug/app-debug.apk`。最低 Android 8.0 / API 26。当前使用 ONNX Runtime Extensions 0.13.0，tokenizer 仍为 4 KB ELF 对齐；未保证 16 KB 内存页设备兼容，不用于直接上架 Google Play。模拟器通过不能代替各品牌真机验收。拼音使用本地 Java 词典，候选、模糊拼音与个人词频能力不等同于桌面 Rime。

Output: `app/build/outputs/apk/debug/app-debug.apk`, Android 8.0/API 26+. The current tokenizer library is 4 KB ELF-aligned; 16 KB page devices are not guaranteed and this debug build is not intended for direct Play Store publication. Emulator validation does not cover every manufacturer. Pinyin uses a local Java dictionary rather than desktop Rime.

测试记录见 [TESTING.md](TESTING.md)。/ See [test results](TESTING.md).

## 来源 / Credits

- 布局操作参考 [搜狗官方使用说明](https://shouji.sogou.com/wap/htmls/instructions.php)，未使用其图标、皮肤或代码。/ Familiar interactions reference Sogou's documentation; no Sogou artwork or code is included.
- 中译英：Helsinki-NLP / Xenova OPUS-MT zh-en，CC BY 4.0，固定 revision `39d480d52a9ea3065a1f117adfe4dbc55de10e6f`。
- 英译中：Helsinki-NLP / Xenova OPUS-MT en-zh，Apache 2.0，固定 revision `046f55aec303cdee3e0318604406d4df20f1e8ea`。
- ONNX Runtime 1.23.2 / Extensions 0.13.0，MIT。模型归属、许可和改动说明随 APK 保存在 `assets/licenses/`。
- 拼音词库来源与 Apache 2.0 许可见 `Resources/RIME-LICENSE.txt`。
