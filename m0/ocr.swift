// M0-WEGWERFCODE: OCR für das gescannte Roland-Handbuch (PDFKit + Vision).
// swift m0/ocr.swift reference/JD-990_OM.pdf reference/ocr   → eine .txt je Seite
import Foundation
import PDFKit
import Vision

let a = CommandLine.arguments
guard a.count == 3, let doc = PDFDocument(url: URL(fileURLWithPath: a[1])) else { print("ocr <pdf> <outdir>"); exit(1) }
try FileManager.default.createDirectory(atPath: a[2], withIntermediateDirectories: true)
for i in 0..<doc.pageCount {
    guard let page = doc.page(at: i) else { continue }
    let box = page.bounds(for: .mediaBox)
    let scale = 3.0
    let img = page.thumbnail(of: CGSize(width: box.width * scale, height: box.height * scale), for: .mediaBox)
    guard let cg = img.cgImage(forProposedRect: nil, context: nil, hints: nil) else { continue }
    let req = VNRecognizeTextRequest()
    req.recognitionLevel = .accurate
    req.usesLanguageCorrection = false
    try VNImageRequestHandler(cgImage: cg).perform([req])
    // Zeilen von oben nach unten, links nach rechts
    let obs = (req.results ?? []).sorted {
        abs($0.boundingBox.midY - $1.boundingBox.midY) > 0.006 ? $0.boundingBox.midY > $1.boundingBox.midY : $0.boundingBox.minX < $1.boundingBox.minX
    }
    let text = obs.compactMap { $0.topCandidates(1).first?.string }.joined(separator: "\n")
    try text.write(toFile: String(format: "%@/p%03d.txt", a[2], i + 1), atomically: true, encoding: .utf8)
    if i % 20 == 0 { print("Seite \(i + 1)/\(doc.pageCount)") }
}
print("fertig")
