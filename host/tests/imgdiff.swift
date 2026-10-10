// Vergleicht zwei PNGs pixelweise (UI-Referenzbilder).
//   swift host/tests/imgdiff.swift <referenz.png> <neu.png> <diff.png> [max. abweichende Pixel, Default 0]
// Exit 0 = gleich, 1 = verschieden. Ein Pixel weicht ab, wenn ein Farbkanal um mehr als 32 Stufen
// abweicht. Default 0: Auf demselben Rechner rendert die App pixelgenau gleich — schon ein
// Wertschritt am Fader (≈ 170 Pixel) oder ein falsches Zeichen (≈ 70 Pixel) muss auffallen.
// diff.png zeigt Abweichungen rot über dem abgedunkelten neuen Bild.
import AppKit

func load(_ path: String) -> (CGImage, [UInt8])? {
    guard let img = NSImage(contentsOfFile: path)?.cgImage(forProposedRect: nil, context: nil, hints: nil) else { return nil }
    let w = img.width, h = img.height
    var px = [UInt8](repeating: 0, count: w * h * 4)
    let ctx = CGContext(data: &px, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4,
                        space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    ctx.draw(img, in: CGRect(x: 0, y: 0, width: w, height: h))
    return (img, px)
}

let a = CommandLine.arguments
guard a.count >= 4, let (ra, pa) = load(a[1]), let (rb, pb) = load(a[2]) else { print("imgdiff <ref> <neu> <diff> [max]"); exit(2) }
let maxBad = a.count > 4 ? Int(a[4]) ?? 0 : 0
guard ra.width == rb.width, ra.height == rb.height else { print("Größe: \(ra.width)×\(ra.height) ≠ \(rb.width)×\(rb.height)"); exit(1) }
var out = pb, bad = 0
for i in stride(from: 0, to: pa.count, by: 4) {
    let d = (0..<3).map { abs(Int(pa[i + $0]) - Int(pb[i + $0])) }.max()!
    if d > 32 { bad += 1; out[i] = 255; out[i + 1] = 0; out[i + 2] = 0 }
    else { for c in 0..<3 { out[i + c] = pb[i + c] / 3 } }
}
let total = ra.width * ra.height
let ratio = Double(bad) / Double(total)
let ctx = CGContext(data: &out, width: ra.width, height: ra.height, bitsPerComponent: 8, bytesPerRow: ra.width * 4,
                    space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
if bad > 0, let img = ctx.makeImage() {
    try? NSBitmapImageRep(cgImage: img).representation(using: .png, properties: [:])?.write(to: URL(fileURLWithPath: a[3]))
}
print(String(format: "%d von %d Pixeln abweichend (%.3f %%)", bad, total, ratio * 100))
exit(bad <= maxBad ? 0 : 1)
