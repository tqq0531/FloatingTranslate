import Foundation
import CoreGraphics

enum PanelPlacement {
    static func isValidAnchor(_ rect: CGRect) -> Bool {
        [rect.minX, rect.minY, rect.width, rect.height].allSatisfy(\.isFinite)
            && rect.height > 0 && rect.height < 500 && rect.width >= 0 && rect.width < 10000
    }
    static func origin(anchor: CGRect, size: CGSize, visibleFrame: CGRect, preferAbove: Bool) -> CGPoint? {
        guard isValidAnchor(anchor), visibleFrame.intersects(anchor), size.width > 0, size.height > 0 else { return nil }
        let gap: CGFloat = 3
        let above = anchor.maxY + gap
        let below = anchor.minY - size.height - gap
        let fitsAbove = above + size.height <= visibleFrame.maxY
        let fitsBelow = below >= visibleFrame.minY
        let proposed = preferAbove ? (fitsAbove ? above : below) : (fitsBelow ? below : above)
        let x = min(max(anchor.minX, visibleFrame.minX + 4), max(visibleFrame.minX, visibleFrame.maxX - size.width - 4))
        let y = min(max(proposed, visibleFrame.minY + 4), max(visibleFrame.minY, visibleFrame.maxY - size.height - 4))
        return CGPoint(x: x, y: y)
    }
}
