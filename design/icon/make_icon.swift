// App-Icon für den JD-990-Editor — gezeichnet, nicht gemalt, damit es reproduzierbar bleibt.
//   swift design/icon/make_icon.swift design/icon/icon-1024.png
// Motiv: grün hinterleuchtetes LCD (Sägezahn + Balkenanzeige) über einer Fader-Wand
// in den Sektionsfarben des Panels (design/panel-prototype.html). Kein Roland-Logo,
// kein Modellname (docs/06-risiken.md, F3).
import AppKit
import CoreGraphics

let size: CGFloat = 1024
let out = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "icon-1024.png"
let cs = CGColorSpaceCreateDeviceRGB()
let ctx = CGContext(data: nil, width: Int(size), height: Int(size), bitsPerComponent: 8, bytesPerRow: 0,
                    space: cs, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
ctx.translateBy(x: 0, y: size); ctx.scaleBy(x: 1, y: -1)   // y nach unten wie in UI-Koordinaten

func rgb(_ hex: UInt32, _ a: CGFloat = 1) -> CGColor {
    CGColor(red: CGFloat((hex >> 16) & 0xFF) / 255, green: CGFloat((hex >> 8) & 0xFF) / 255, blue: CGFloat(hex & 0xFF) / 255, alpha: a)
}
func rounded(_ r: CGRect, _ radius: CGFloat) -> CGPath { CGPath(roundedRect: r, cornerWidth: radius, cornerHeight: radius, transform: nil) }
func linear(_ path: CGPath, _ c1: CGColor, _ c2: CGColor, from: CGPoint, to: CGPoint) {
    ctx.saveGState(); ctx.addPath(path); ctx.clip()
    let g = CGGradient(colorsSpace: cs, colors: [c1, c2] as CFArray, locations: [0, 1])!
    ctx.drawLinearGradient(g, start: from, end: to, options: [])
    ctx.restoreGState()
}

// ── Gehäuse (macOS-Raster: 824 × 824, Radius ~185) ─────────────────────────────
let body = CGRect(x: 100, y: 100, width: 824, height: 824)
ctx.saveGState()
ctx.setShadow(offset: CGSize(width: 0, height: 18), blur: 36, color: rgb(0x000000, 0.45))
ctx.addPath(rounded(body, 185)); ctx.setFillColor(rgb(0x212429)); ctx.fillPath()
ctx.restoreGState()
linear(rounded(body, 185), rgb(0x30343c), rgb(0x16181c), from: CGPoint(x: 0, y: 100), to: CGPoint(x: 0, y: 924))
// Kante oben (Licht)
ctx.saveGState(); ctx.addPath(rounded(body.insetBy(dx: 2, dy: 2), 183)); ctx.setStrokeColor(rgb(0xffffff, 0.08)); ctx.setLineWidth(4); ctx.strokePath(); ctx.restoreGState()

// ── LCD ────────────────────────────────────────────────────────────────────────
let bezel = CGRect(x: 186, y: 196, width: 652, height: 268)
ctx.addPath(rounded(bezel, 26)); ctx.setFillColor(rgb(0x0b0d0a)); ctx.fillPath()
let screen = bezel.insetBy(dx: 22, dy: 22)
linear(rounded(screen, 10), rgb(0xb4cf6a), rgb(0x87a345), from: CGPoint(x: screen.minX, y: screen.minY), to: CGPoint(x: screen.maxX, y: screen.maxY))
// Glanz
ctx.saveGState(); ctx.addPath(rounded(screen, 10)); ctx.clip()
let glow = CGGradient(colorsSpace: cs, colors: [rgb(0xffffff, 0.28), rgb(0xffffff, 0)] as CFArray, locations: [0, 1])!
ctx.drawRadialGradient(glow, startCenter: CGPoint(x: screen.minX + 150, y: screen.minY + 30), startRadius: 0,
                       endCenter: CGPoint(x: screen.minX + 150, y: screen.minY + 30), endRadius: 360, options: [])
// Pixelraster
ctx.setFillColor(rgb(0x000000, 0.035))
var y = screen.minY; while y < screen.maxY { ctx.fill(CGRect(x: screen.minX, y: y, width: screen.width, height: 2)); y += 8 }
ctx.restoreGState()

// Sägezahn (Oszillator) in LCD-Tinte
let ink = rgb(0x161c0e)
ctx.saveGState()
ctx.setStrokeColor(ink); ctx.setLineWidth(15); ctx.setLineJoin(.miter); ctx.setLineCap(.square)
let w0 = screen.minX + 44, wy0 = screen.minY + 128, wy1 = screen.minY + 34, period: CGFloat = 132
ctx.move(to: CGPoint(x: w0, y: wy0))
for i in 0..<4 {
    let x = w0 + CGFloat(i) * period
    ctx.addLine(to: CGPoint(x: x + period, y: wy1))
    if i < 3 { ctx.addLine(to: CGPoint(x: x + period, y: wy0)) }
}
ctx.strokePath()
ctx.restoreGState()

// Balkenanzeige
let segs = 18, barY = screen.maxY - 44, barW = (screen.width - 88) / CGFloat(segs)
for i in 0..<segs {
    ctx.setFillColor(i < 12 ? ink : rgb(0x161c0e, 0.18))
    ctx.fill(CGRect(x: screen.minX + 44 + CGFloat(i) * barW, y: barY, width: barW - 7, height: 22))
}

// Akzentstreifen in der Farbe der 990-Extras (design/panel-prototype.html, --a-990)
ctx.addPath(rounded(CGRect(x: 186, y: 492, width: 652, height: 10), 5)); ctx.setFillColor(rgb(0xe0603f)); ctx.fillPath()

// ── Fader-Wand ─────────────────────────────────────────────────────────────────
let colors: [UInt32] = [0xd9a441, 0x5aa9d6, 0x6cc47a, 0xd4708f, 0x9d86d9]   // WG, Pitch, TVF, TVA, LFO
let levels: [CGFloat] = [0.72, 0.38, 0.86, 0.55, 0.28]
let trackTop: CGFloat = 540, trackBottom: CGFloat = 848, step: CGFloat = 128
for (i, c) in colors.enumerated() {
    let cx = 256 + CGFloat(i) * step
    // Schiene
    let rail = CGRect(x: cx - 13, y: trackTop, width: 26, height: trackBottom - trackTop)
    ctx.addPath(rounded(rail, 6)); ctx.setFillColor(rgb(0x0f1114)); ctx.fillPath()
    let capY = trackBottom - levels[i] * (trackBottom - trackTop)
    // Füllung
    ctx.setFillColor(rgb(c, 0.9)); ctx.fill(CGRect(x: cx - 6, y: capY, width: 12, height: trackBottom - capY - 4))
    // Kappe
    let cap = CGRect(x: cx - 46, y: capY - 22, width: 92, height: 44)
    ctx.saveGState()
    ctx.setShadow(offset: CGSize(width: 0, height: 6), blur: 10, color: rgb(0x000000, 0.6))
    ctx.addPath(rounded(cap, 8)); ctx.setFillColor(rgb(0xc8c4bb)); ctx.fillPath()
    ctx.restoreGState()
    linear(rounded(cap, 8), rgb(0xe2ded5), rgb(0x9a968e), from: CGPoint(x: 0, y: cap.minY), to: CGPoint(x: 0, y: cap.maxY))
    ctx.setFillColor(rgb(0x000000, 0.5)); ctx.fill(CGRect(x: cap.minX + 10, y: cap.midY - 2, width: cap.width - 20, height: 4))
    // Farbmarke auf der Kappe
    ctx.setFillColor(rgb(c)); ctx.fill(CGRect(x: cap.minX + 10, y: cap.minY + 6, width: cap.width - 20, height: 6))
}

let img = ctx.makeImage()!
let rep = NSBitmapImageRep(cgImage: img)
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: out))
print("→ \(out)")
