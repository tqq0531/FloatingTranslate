import AppKit
import CoreText

// Native input-mode keycaps use a 22 x 16 point canvas. PDF keeps a single
// logical viewport for the menu bar, input-source menu and switch indicator.
let size = CGSize(width: 22, height: 16)
let font = CTFontCreateWithName("PingFangSC-Semibold" as CFString, 14, nil)
var character: UniChar = 0x8BD1
var glyph: CGGlyph = 0
guard CTFontGetGlyphsForCharacters(font, &character, &glyph, 1),
      let path = CTFontCreatePathForGlyph(font, glyph, nil) else { fatalError("Cannot create input-mode glyph") }
let ink = path.boundingBoxOfPath
let fit = 12.5 / max(ink.width, ink.height)
var transform = CGAffineTransform(a: fit, b: 0, c: 0, d: fit,
    tx: size.width / 2 - ink.midX * fit, ty: size.height / 2 - ink.midY * fit)
guard let centeredGlyph = path.copy(using: &transform) else { fatalError("Cannot center glyph") }
let keycap = CGMutablePath()
keycap.addRoundedRect(in: CGRect(origin: .zero, size: size), cornerWidth: 4, cornerHeight: 4)
keycap.addPath(centeredGlyph)
var mediaBox = CGRect(origin: .zero, size: size)
let url = URL(fileURLWithPath: CommandLine.arguments[1])
guard let pdf = CGContext(url as CFURL, mediaBox: &mediaBox, nil) else { fatalError("Cannot encode menu icon") }
pdf.beginPDFPage(nil)
pdf.setFillColor(CGColor(gray: 0, alpha: 1))
pdf.addPath(keycap)
pdf.drawPath(using: .eoFill) // Transparent glyph, tinted automatically by macOS.
pdf.endPDFPage(); pdf.closePDF()
