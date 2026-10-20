# 浮译 App 图标 · v1

设计方向：悬浮的中英对话卡片。冰蓝与深蓝圆角底牌，后方“译”卡片和前方“A”卡片表达输入与翻译。图标正视、轻微玻璃层次，外部透明，为 macOS 留出安全边距。

- 原始图像：`fuyi-app-icon-v2.png`
- macOS 图标：`../../Resources/FuyiApp.icns`
- 重建：项目根目录运行 `scripts/make-app-icon.sh`；正常构建也会自动生成。
- 尺寸：16、32、128、256、512 点，均包含 @2x。
- 制作方式：内置 image_gen；缩放与 ICNS 打包使用系统 sips / iconutil。

## 生成提示词

Use case: logo-brand. Design a finished premium native macOS app icon for 浮译 (Fuyi), a local Chinese/English typing and floating translation utility. Single icon, square 1024x1024 composition, transparent outside the rounded-square icon silhouette, ample 7% outer transparent safe margin. Rounded-square tile in luminous deep azure and soft ice-blue, subtle tasteful translucent glass depth and soft inner highlights, clean confident contemporary Mac utility aesthetic. In center two slightly overlapping floating speech cards, back card frosted pale blue displaying exactly the single Chinese character “译” in crisp centered bold typography, foreground card smaller porcelain-white displaying exactly “A” in dark azure. Balanced optical alignment, generously readable central glyphs, restrained soft shadow, very simple distinct silhouette readable at 32 pixels. Front-on orthographic view, no perspective tilt of app tile, no surrounding scene, no mockup, no border decorations, no tiny text, no arrows, no watermark. Deliver one actual usable app icon, not a presentation board.

## 边缘清理提示词

Precise cleanup of the supplied macOS icon. Keep the entire blue rounded square tile, both speech cards, the exact 译 and A lettering, composition and materials unchanged. Remove ALL stray colored/white pixels, flecks and disconnected fragments in the transparent outer margin, especially above the top edge. Outside the clean rounded square silhouette and its restrained soft shadow must be perfectly transparent with no floating debris. Preserve sharp tidy antialiased perimeter. Output square transparent PNG of the same app icon. No additions.
