# 浮译 Android · 0.1.3 试用版

原生安卓输入法，支持本地拼音候选、分段输入后的整句英文预览，以及保留 24 小时的文字剪贴板历史。

## 安装和使用

1. 安装 APK，打开“浮译”。试用 APK 使用开发签名，尚未上架应用商店。
2. 点击“启用键盘”，在系统设置启用；再点“切换输入法”。Android 会展示所有第三方键盘的标准风险提示。
3. 在试用输入框输入 `nihao`，按空格确认“你好”。通过底部“中/英 / EN”按钮切换直接英文；`123` 打开数字符号。
4. 在应用设置通过 Wi-Fi 下载中英离线语言包。下载完成后，键盘上方显示本机英文译文，长按可复制整句。正文不会发到开发者服务器。
5. 开启“复制历史”。在其他软件复制文字，回到任意普通文本框，点击键盘“剪贴板”，点某条历史即可插入；每条右侧“⋮”可以查看到期时间或删除，顶部“⋯”可以“清空”。关闭保存开关立即清空历史。

## 设置界面

0.1.3 重做设置页：深蓝引导区、白色卡片、统一按钮和开关，按输入偏好、本机翻译、试用和隐私分组。完整隐私说明可展开；网络边界与敏感输入建议直接展示。已安装的语言包会显示“已就绪”，正在下载时保留实际进度。

[设置首页](docs/settings-home.png) · [翻译、试用与隐私](docs/settings-details.png) · [窄屏与大字号](docs/settings-large-text.png)

## 语言包下载进度

0.1.2 新增水平进度条，显示系统下载器实际返回的百分比、已下载 MB 和总大小。连接服务或总大小未知时显示等待动画；等待 Wi-Fi、网络恢复、重试和校验安装有独立提示。下载达到 100% 后仍需完成校验，只有翻译 SDK 确认成功才提示“已就绪”。

下载期间关闭设置页再打开，可恢复正在进行的下载状态；不会重复创建应用自己的下载任务。失败后可点击重试。读取范围仅限本应用的中英模型下载，不新增存储权限。部分系统限制进度查询或 SDK 更改下载地址时，显示无法读取大小的等待状态，继续等待 SDK 结果，不编造百分比。

[真实下载进度截图](docs/download.png)

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

翻译使用 Google ML Kit 本机模型，正文和译文在设备处理，不发送给开发者或作为云翻译请求。**不能称为完全不联网**：首次准备语言包需要网络，ML Kit 也可能连接 Google 下载更新、接收兼容性信息，并发送使用与性能指标。在无法访问其下载服务的设备上，离线模型可能准备失败，键盘和剪贴板功能仍可使用。没有在线翻译备用接口。

“拼音转换后默认输出英文”只在本次待确认内容能安全直接提交时生效。前面的词已按中文上屏后，长按复制整句英文；不自动删除并替换已上屏的历史正文。只累计本次通过浮译输入的当前句子，不扫描输入框前文；切换输入框、移动光标或超时会清理这段内存。

## 开发

使用 JDK 17/21、Android SDK 36、Build Tools 36.1.0。Gradle Wrapper 固定 8.13，AGP 固定 8.13.2，ML Kit translate 固定 17.0.3。构建和首次下载依赖需要联网。

```sh
cd Android
# local.properties 填入本机 sdk.dir，或设置 ANDROID_HOME
./gradlew :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
# 已启动独立模拟器/连接专门测试机，并在该设备启用浮译后：
./gradlew :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.fuyi.keyboard.ClipboardDeviceTest,com.fuyi.keyboard.KeyboardDeviceTest,com.fuyi.keyboard.DownloadProgressDeviceTest,com.fuyi.keyboard.SettingsDeviceTest
# 单独验证模型下载和真实翻译（明确允许下载约 35 MB 模型）：
./gradlew :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.fuyi.keyboard.TranslationModelDeviceTest \
  -Pandroid.testInstrumentationRunnerArguments.downloadModels=true
# 模型准备完成后，在测试设备断网，再验证纯本机翻译：
./gradlew :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=com.fuyi.keyboard.TranslationModelDeviceTest \
  -Pandroid.testInstrumentationRunnerArguments.preparedModel=true
```

APK：`app/build/outputs/apk/debug/app-debug.apk`。最低 Android 8.0 / API 26。模型支持和键盘兼容性还需在实际手机上验收，不能将模拟器通过当成全部厂商手机通过。

单元测试覆盖真实词库、简拼、句子切分、错拼、24 小时边界、重复复制不续期、空白/未来时间/超长拒绝和数量上限。设备测试覆盖 Keystore 加密、重开持久化、过期不可选、单条删除、密文篡改、密码字段和敏感剪贴板跳过，并通过真实软键盘点击验证分段整句上下文、中英文切换和历史插入。模型测试只有显式传入 `downloadModels=true` 才会下载，普通测试不主动准备语言包。

验收详情和当前限制见 [测试记录](TESTING.md)。

## 参考和许可

- [Android 默认输入法的剪贴板访问限制](https://developer.android.com/about/versions/10/privacy/changes#clipboard-data)
- [敏感剪贴板标记](https://developer.android.com/develop/ui/views/touch-and-input/copy-paste#sensitive-content)
- [ML Kit 本机翻译](https://developers.google.com/ml-kit/language/translation/android) 与 [隐私说明](https://developers.google.com/ml-kit/terms)
- 拼音词库来源与 Apache 2.0 许可见仓库 `Resources/RIME-LICENSE.txt`。ML Kit 属独立第三方 SDK，按其条款使用。

下载进度依据 [Android DownloadManager](https://developer.android.com/reference/android/app/DownloadManager) 的实际字节数；模型就绪依据 [ML Kit Translator](https://developers.google.com/android/reference/com/google/mlkit/nl/translate/Translator) 的下载任务结果。
