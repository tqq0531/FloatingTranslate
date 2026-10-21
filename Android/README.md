# 浮译 Android · 0.1.4 内置离线模型试用版

原生安卓输入法，支持本地拼音候选、分段输入后的整句英文预览，以及保留 24 小时的文字剪贴板历史。

## 安装和使用

1. 安装 APK，打开“浮译”。试用 APK 使用开发签名，尚未上架应用商店。
2. 点击“启用键盘”，在系统设置启用；再点“切换输入法”。Android 会展示所有第三方键盘的标准风险提示。
3. 在试用输入框输入 `nihao`，按空格确认“你好”。通过底部“中/英 / EN”按钮切换直接英文；`123` 打开数字符号。
4. 中译英模型已打进 APK。首次打开自动从安装包解压、校验并加载，无需网络、Google 服务或另下语言包；设置页会显示实际解压百分比和 MB。键盘上方的整句英文可长按复制。若初始化失败，保留至少 500 MB 可用空间后在设置重试。
5. 开启“复制历史”。在其他软件复制文字，回到任意普通文本框，点击键盘“剪贴板”，点某条历史即可插入；每条右侧“⋮”可以查看到期时间或删除，顶部“⋯”可以“清空”。关闭保存开关立即清空历史。

## 设置界面

0.1.3 重做设置页：深蓝引导区、白色卡片、统一按钮和开关，按输入偏好、本机翻译、试用和隐私分组。完整隐私说明可展开；网络边界与敏感输入建议直接展示。内置模型初始化成功后显示“已就绪”，提取模型时显示实际进度。

[设置首页](docs/settings-home.png) · [翻译、试用与隐私](docs/settings-details.png) · [窄屏与大字号](docs/settings-large-text.png)

## 内置离线翻译

0.1.4 替换原 Google ML Kit 翻译组件，使用 OPUS-MT 中译英 int8 模型与 ONNX Runtime，在手机 CPU 上推理。模型约 110 MB，随 APK 一起安装；安装后没有模型下载步骤，国内网络无法访问 Google 不影响使用。旧版系统下载任务不会继续供新版使用，新版也不读取其模型缓存。

首次初始化从 APK 提取模型到本应用私有的 no-backup 目录，按清单逐个验证 SHA-256，并显示真实字节进度。到 100% 后仍需加载推理引擎，加载成功才显示“已就绪”。损坏或中断的文件会在下次初始化时重新校验、提取。卸载会删除提取文件，重装无需联网，重开设置可检查状态。

当前只做**中文 → 英文**翻译；“中英文切换”指中文拼音与直接英文输入切换，不是双向机器翻译。输入最多 256 个模型子词，输出最多 128 个子词，超过时提示分句，不静默截断。只做本机贪心解码，翻译质量与旧版不同，专名、口语和长句可能不准；速度和内存占用需按实际手机验收。

采用预编译的 ONNX Runtime Extensions 0.13.0，其中 tokenizer 库仍为 4 KB ELF 对齐。当前已验证设备为 4 KB 页系统；**尚不承诺 16 KB 页设备兼容，也不用于直接上架 Google Play**。后续需要重编译此依赖并在 16 KB 设备测试，不能只通过 APK zipalign 当作已经支持。

## 键盘布局与手感

0.1.1 按常用手机键盘重新排布：灰底白键、第二排半键缩进，第三排两侧是 Shift 和退格，底部为数字符号、表情、逗号、宽空格、中英和绿色操作键。操作键根据输入框显示发送、搜索、完成或换行。

按键可见间隙属于触摸范围；按下有颜色反馈与系统轻震动（受设备设置控制），退格按住约 0.38 秒开始连续删除。Shift 切换下一字的大写，字母键默认用大写键帽显示，不代表实际输入大写。剪贴板采用整行白色卡片，保留原来的本地加密及 24 小时规则。

[键盘样式](docs/layout.png) · [剪贴板样式](docs/clipboard.png)

## 历史记录的保留规则

- 从开启功能之后开始收集**文字**，无法还原开启之前的所有系统复制记录。剪贴板图片、文件和 URI 不导入。
- 最多 100 条，每条不超过 10,000 个 UTF-16 字符；超限长文本会跳过，超过数量上限淘汰最旧的记录。
- 每条从其复制时间起保留 24 小时。到期立即不再显示或允许插入。重复复制同样文字不延长已有记录的期限。
- AES-GCM 加密保存到应用私有的 no-backup 目录；密钥由 Android Keystore 管理。没有云同步，不允许系统备份和设备迁移。
- 程序重开后仍能读取未过期记录；打开应用、显示/选用历史和后台过期任务均会清理。Android 休眠、关机、强行停止可能推迟磁盘文件的物理清理，但再次访问会先按 24 小时规则过滤。
- 只有浮译为当前默认输入法、系统允许访问且服务存活时才可能记录。切到其他键盘或被系统停止期间不保证收集；不是无间断的系统剪贴板审计工具。
- 清空浮译历史不会改动系统当前剪贴板；操作系统自己的剪贴板保留规则独立生效。

## 密码和网络边界

密码/可见密码/Web 密码/数字密码输入框，以及带“不允许个性化学习”标志的输入框，不翻译、不展示历史，跳过此时的复制。来源应用给剪贴板标记 `EXTRA_IS_SENSITIVE` 时也跳过。未正确标记的自定义密码框、未带敏感标记的口令仍可能无法识别；密码、验证码、密钥请切回系统键盘，不要保存到历史。

拼音使用同仓库的固定本地词库和 Java 词典实现；安卓首版没有集成 native Rime，因此候选排序、模糊拼音和个人词频能力与 Mac 版不同，也不会学习输入历史。支持全拼、部分首字母简拼和相邻字母对调纠错，不承诺任意错拼。

**Android 0.1.4 内置版没有 `INTERNET` 权限，已移除 Google ML Kit、远程模型下载及其指标上报组件。** 中文和英文只经过 APK 内置模型处理，不发给 Google、国内云翻译 API 或开发者服务器。没有统计 SDK、云翻译后备接口或输入日志。运行时模型初始化和翻译均可断网完成。操作系统、商店和其他应用的联网行为不属于本应用控制范围；调试 APK 仅供试用，不代表系统或第三方键盘绝无风险。

“拼音转换后默认输出英文”只在本次待确认内容能安全直接提交时生效。前面的词已按中文上屏后，长按复制整句英文；不自动删除并替换已上屏的历史正文。只累计本次通过浮译输入的当前句子，不扫描输入框前文；切换输入框、移动光标或超时会清理这段内存。

## 开发

使用 JDK 17/21、Android SDK 36、Build Tools 36.1.0。Gradle Wrapper 固定 8.13，AGP 固定 8.13.2，ONNX Runtime 1.23.2，Extensions 0.13.0。**开发电脑**首次拉取依赖和模型需要联网，手机用户安装完整 APK 后不需要。大型模型不提交进 Git；清单、导出脚本和许可随仓库提供，Gradle 会检查每个必需文件的大小和 SHA-256，缺失或不符即停止构建，不会生成缺模型的 APK。

```sh
cd Android
python3 -m venv .model-venv
.model-venv/bin/pip install -r tools/requirements-translation.txt
.model-venv/bin/python tools/prepare_translation.py
# local.properties 填入本机 sdk.dir，或设置 ANDROID_HOME
./gradlew :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
# 在专用模拟器/测试机安装后，关闭 Wi-Fi 和移动网络再首次启动：
./gradlew :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.fuyi.keyboard.TranslationModelDeviceTest,com.fuyi.keyboard.DownloadProgressDeviceTest,com.fuyi.keyboard.ClipboardDeviceTest,com.fuyi.keyboard.KeyboardDeviceTest,com.fuyi.keyboard.SettingsDeviceTest \
  -Pandroid.testInstrumentationRunnerArguments.preparedModel=true
```

APK：`app/build/outputs/apk/debug/app-debug.apk`。最低 Android 8.0 / API 26。包含 arm64-v8a、armeabi-v7a、x86_64、x86 推理库；实际验收范围见测试记录，不能将模拟器通过当作全部厂商手机通过。

模型来源固定在 `Xenova/opus-mt-zh-en` 的 `39d480d52a9ea3065a1f117adfe4dbc55de10e6f`；基础模型为 Helsinki-NLP/opus-mt-zh-en。导出脚本生成原 SentencePiece 分词器的 ONNX 包装及词表映射，模型权重不改写。构建产物包含模型归属、修改说明、CC BY 4.0、运行时 MIT 许可及原模型卡，见 `app/src/main/assets/licenses/`。

单元测试覆盖真实词库、简拼、错拼、24 小时边界和真实字节进度。设备测试覆盖断网初始化/真实翻译、APK 无网络权限、英文复制与提交、分段整句、键盘手势和剪贴板安全规则。模型测试不再下载任何内容。

验收详情和当前限制见 [测试记录](TESTING.md)。

## 参考和许可

- [Android 默认输入法的剪贴板访问限制](https://developer.android.com/about/versions/10/privacy/changes#clipboard-data)
- [敏感剪贴板标记](https://developer.android.com/develop/ui/views/touch-and-input/copy-paste#sensitive-content)
- [OPUS-MT 基础模型与模型卡](https://huggingface.co/Helsinki-NLP/opus-mt-zh-en)（CC BY 4.0）和 [固定 ONNX 转换来源](https://huggingface.co/Xenova/opus-mt-zh-en/tree/39d480d52a9ea3065a1f117adfe4dbc55de10e6f)
- [ONNX Runtime Java](https://onnxruntime.ai/docs/get-started/with-java.html) 与 [Extensions](https://onnxruntime.ai/docs/extensions/)（MIT）
- 拼音词库来源与 Apache 2.0 许可见仓库 `Resources/RIME-LICENSE.txt`。各组件保持自己的许可证，不以本仓库许可覆盖。
