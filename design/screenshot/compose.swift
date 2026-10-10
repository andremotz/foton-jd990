// README-Screenshot: App-Aufnahme in ein macOS-Fenster (Titelleiste, Ampel, Schatten)
// auf einem angedeuteten macOS-Hintergrund setzen — im Stil von Foton Kanban.
//   swift design/screenshot/compose.swift <app.png> <out.png> [Titel]
import AppKit

let args = CommandLine.arguments
guard args.count >= 3, let shot = NSImage(contentsOfFile: args[1]),
      let shotCG = shot.cgImage(forProposedRect: nil, context: nil, hints: nil) else { print("compose <app.png> <out.png> [Titel]"); exit(1) }
let title = args.count > 3 ? args[3] : "FotonJD990"

let W = 2400, H = 1468
let margin: CGFloat = 105, titleH: CGFloat = 60, radius: CGFloat = 26
let winW = CGFloat(W) - 2 * margin
let contentH = winW * CGFloat(shotCG.height) / CGFloat(shotCG.width)
let winH = titleH + contentH
let winY = (CGFloat(H) - winH) / 2 - 4                       // Ursprung unten links (CoreGraphics)
let win = CGRect(x: margin, y: winY, width: winW, height: winH)

let cs = CGColorSpace(name: CGColorSpace.sRGB)!
let ctx = CGContext(data: nil, width: W, height: H, bitsPerComponent: 8, bytesPerRow: 0, space: cs,
                    bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
func rgb(_ r: CGFloat, _ g: CGFloat, _ b: CGFloat, _ a: CGFloat = 1) -> CGColor { CGColor(colorSpace: cs, components: [r, g, b, a])! }

// Hintergrund: Blauverlauf mit weichen diagonalen Bahnen (macOS-artig, eigene Grafik)
let bg = CGGradient(colorsSpace: cs, colors: [rgb(0.42, 0.68, 0.88), rgb(0.20, 0.47, 0.76), rgb(0.06, 0.24, 0.50)] as CFArray, locations: [0, 0.5, 1])!
ctx.drawLinearGradient(bg, start: CGPoint(x: 0, y: CGFloat(H)), end: CGPoint(x: CGFloat(W), y: 0), options: [])
for (i, alpha) in [0.10, 0.07, 0.05, 0.08].enumerated() {
    let off = CGFloat(i) * 520 - 300
    let band = CGMutablePath()
    band.move(to: CGPoint(x: off, y: 0))
    band.addLine(to: CGPoint(x: off + 700, y: 0))
    band.addLine(to: CGPoint(x: off + 1700, y: CGFloat(H)))
    band.addLine(to: CGPoint(x: off + 1250, y: CGFloat(H)))
    band.closeSubpath()
    ctx.addPath(band)
    ctx.setFillColor(rgb(1, 1, 1, CGFloat(alpha)))
    ctx.fillPath()
}

// Fensterschatten
let winPath = CGPath(roundedRect: win, cornerWidth: radius, cornerHeight: radius, transform: nil)
ctx.saveGState()
ctx.setShadow(offset: CGSize(width: 0, height: -22), blur: 70, color: rgb(0, 0, 0, 0.45))
ctx.addPath(winPath)
ctx.setFillColor(rgb(0.13, 0.14, 0.16))
ctx.fillPath()
ctx.restoreGState()

// Inhalt (abgerundet beschnitten)
ctx.saveGState()
ctx.addPath(winPath)
ctx.clip()
ctx.setFillColor(rgb(0.165, 0.18, 0.21))                      // Titelleiste wie das Panel (#2b2f36)
ctx.fill(CGRect(x: win.minX, y: win.maxY - titleH, width: winW, height: titleH))
ctx.interpolationQuality = .high
ctx.draw(shotCG, in: CGRect(x: win.minX, y: win.minY, width: winW, height: contentH))
ctx.setFillColor(rgb(0, 0, 0, 0.35))
ctx.fill(CGRect(x: win.minX, y: win.maxY - titleH, width: winW, height: 1.5))
ctx.restoreGState()

// Ampel
for (i, c) in [rgb(1.0, 0.37, 0.34), rgb(1.0, 0.74, 0.18), rgb(0.16, 0.79, 0.25)].enumerated() {
    let r: CGFloat = 12
    let cx = win.minX + 34 + CGFloat(i) * 36, cy = win.maxY - titleH / 2
    ctx.setFillColor(c)
    ctx.fillEllipse(in: CGRect(x: cx - r, y: cy - r, width: 2 * r, height: 2 * r))
}

// Titel
let gc = NSGraphicsContext(cgContext: ctx, flipped: false)
NSGraphicsContext.current = gc
let attrs: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 26, weight: .semibold), .foregroundColor: NSColor(white: 0.86, alpha: 1)]
let t = NSAttributedString(string: title, attributes: attrs)
let ts = t.size()
t.draw(at: CGPoint(x: win.midX - ts.width / 2, y: win.maxY - titleH / 2 - ts.height / 2))

// Feiner Fensterrand
ctx.addPath(winPath)
ctx.setStrokeColor(rgb(1, 1, 1, 0.14))
ctx.setLineWidth(1.5)
ctx.strokePath()

let rep = NSBitmapImageRep(cgImage: ctx.makeImage()!)
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: args[2]))
print("→ \(args[2]) (\(W)×\(H))")
