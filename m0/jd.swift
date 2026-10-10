// ─────────────────────────────────────────────────────────────────────────────
//  M0-WEGWERFCODE — kein Produktivcode (docs/05-meilensteine.md, M0).
//  Nur lesende Anfragen (RQ1). Dieses Tool sendet KEIN DT1.
//
//  swift m0/jd.swift probe                     Patchname im Temp-Bereich lesen
//  swift m0/jd.swift rq 03000000 16            RQ1: Adresse (hex, 4 Byte), Größe (dez)
//  swift m0/jd.swift backup corpus/<datum>     System, 64 Patches, Performances → .syx
//
//  Umgebung: JD_PORT (Default "Unitor8/AMT8 Port 2"), JD_DEV (Default 16 = 0x10)
// ─────────────────────────────────────────────────────────────────────────────
import CoreMIDI
import Foundation

let env = ProcessInfo.processInfo.environment
let portName = env["JD_PORT"] ?? "Unitor8/AMT8 Port 2"
let dev = UInt8(env["JD_DEV"].flatMap { Int($0) } ?? 0x10)

// ── MIDI ─────────────────────────────────────────────────────────────────────

func name(_ o: MIDIObjectRef) -> String {
    var s: Unmanaged<CFString>?
    MIDIObjectGetStringProperty(o, kMIDIPropertyDisplayName, &s)
    return (s?.takeRetainedValue() as String?) ?? "?"
}
func find(_ n: Int, _ get: (Int) -> MIDIEndpointRef) -> MIDIEndpointRef? {
    (0..<n).map(get).first { name($0) == portName }
}
guard let dst = find(MIDIGetNumberOfDestinations(), { MIDIGetDestination($0) }),
      let src = find(MIDIGetNumberOfSources(), { MIDIGetSource($0) }) else {
    FileHandle.standardError.write("Port nicht gefunden: \(portName)\n".data(using: .utf8)!); exit(1)
}

let lock = NSLock()
var partial: [UInt8] = []
var received: [[UInt8]] = []
var lastByteAt = Date.distantPast

var client = MIDIClientRef(); MIDIClientCreate("jd990-m0" as CFString, nil, nil, &client)
var inPort = MIDIPortRef()
MIDIInputPortCreateWithBlock(client, "in" as CFString, &inPort) { list, _ in
    for p in list.unsafeSequence() {
        let bytes = withUnsafeBytes(of: p.pointee.data) { Array($0.prefix(Int(p.pointee.length))) }
        lock.lock()
        lastByteAt = Date()
        for b in bytes {
            if b == 0xF0 { partial = [b] }
            else if !partial.isEmpty { partial.append(b); if b == 0xF7 { received.append(partial); partial = [] } }
        }
        lock.unlock()
    }
}
MIDIPortConnectSource(inPort, src, nil)
var outPort = MIDIPortRef(); MIDIOutputPortCreate(client, "out" as CFString, &outPort)

func send(_ msg: [UInt8]) {
    // Eigener Puffer: MIDIPacketList() auf dem Stack fasst nur ~256 Datenbytes — ein DT1 mit
    // 256 Nutzbytes (267 Byte gesamt) wurde damit STILL verworfen (Logbuch 2026-09-28).
    let size = 1024 + msg.count
    let mem = UnsafeMutableRawPointer.allocate(byteCount: size, alignment: 8)
    defer { mem.deallocate() }
    let list = mem.bindMemory(to: MIDIPacketList.self, capacity: 1)
    let pkt = MIDIPacketListInit(list)
    guard MIDIPacketListAdd(list, size, pkt, 0, msg.count, msg) != nil else {
        fatalError("MIDIPacketListAdd fehlgeschlagen (\(msg.count) Byte)")
    }
    MIDISend(outPort, dst, list)
}

// ── Roland ───────────────────────────────────────────────────────────────────

func hex(_ b: [UInt8]) -> String { b.map { String(format: "%02X", $0) }.joined(separator: " ") }
func rolandSum<C: Collection>(_ b: C) -> UInt8 where C.Element == UInt8 {
    UInt8((128 - b.reduce(0) { $0 + Int($1) } % 128) % 128)
}
func be7(_ v: Int) -> [UInt8] { [UInt8((v >> 21) & 0x7F), UInt8((v >> 14) & 0x7F), UInt8((v >> 7) & 0x7F), UInt8(v & 0x7F)] }
func addr7(_ b: [UInt8]) -> Int { b.reduce(0) { ($0 << 7) | Int($1) } }

struct DT1 { let addr: Int; let data: [UInt8]; let raw: [UInt8]; let sumOK: Bool }
func parseDT1(_ m: [UInt8]) -> DT1? {
    guard m.count > 10, m[1] == 0x41, m[2] == dev, m[3] == 0x57, m[4] == 0x12 else { return nil }
    return DT1(addr: addr7(Array(m[5..<9])), data: Array(m[9..<(m.count - 2)]), raw: m,
               sumOK: rolandSum(m[5..<(m.count - 2)]) == m[m.count - 2])
}

/// RQ1 senden, DT1-Antworten sammeln bis `expect` Bytes da sind oder `quiet` s Funkstille.
func request(_ address: Int, _ size: Int, expect: Int? = nil, quiet: Double = 0.4, timeout: Double = 8) -> [DT1] {
    let body = be7(address) + be7(size)
    let msg: [UInt8] = [0xF0, 0x41, dev, 0x57, 0x11] + body + [rolandSum(body), 0xF7]
    lock.lock(); received.removeAll(); partial.removeAll(); lastByteAt = Date(); lock.unlock()
    send(msg)
    let start = Date()
    while true {
        Thread.sleep(forTimeInterval: 0.02)
        lock.lock()
        let got = received.compactMap(parseDT1)
        let idle = Date().timeIntervalSince(lastByteAt)
        lock.unlock()
        let bytes = got.reduce(0) { $0 + $1.data.count }
        if let e = expect, bytes >= e, idle > 0.05 { return got }
        if !got.isEmpty && idle > quiet { return got }
        if Date().timeIntervalSince(start) > timeout { return got }
    }
}

// ── Lesen / Schreiben (Nachtbetrieb, internes Runbook) ────────────────

/// Liest einen Block zusammenhängend; nil bei Lücke, Prüfsummenfehler oder falscher Länge.
func readBlock(_ address: Int, _ size: Int) -> [UInt8]? {
    let r = request(address, size, expect: size).sorted { $0.addr < $1.addr }
    var out: [UInt8] = []
    for d in r {
        guard d.sumOK, d.addr == address + out.count else { return nil }
        out += d.data
    }
    return out.count == size ? out : nil
}

/// Zerlegt eine .syx-Datei in Einzelnachrichten.
func splitSysex(_ bytes: [UInt8]) -> [[UInt8]] {
    var msgs: [[UInt8]] = [], cur: [UInt8] = []
    for b in bytes {
        if b == 0xF0 { cur = [b] } else if !cur.isEmpty { cur.append(b); if b == 0xF7 { msgs.append(cur); cur = [] } }
    }
    return msgs
}

/// HARTE GRENZE: DT1 nur in den Temp-Patch-Bereich (oberstes Adressbyte 0x03).
func dt1(_ address: Int, _ data: [UInt8]) {
    let a = be7(address)
    precondition(a[0] == 0x03, "VERWEIGERT: DT1 außerhalb Temp Patch (\(hex(a)))")
    precondition(data.allSatisfy { $0 < 0x80 }, "VERWEIGERT: Datenbyte ≥ 0x80")
    let body = a + data
    send([0xF0, 0x41, dev, 0x57, 0x12] + body + [rolandSum(body), 0xF7])
}

/// Schreibt Daten ab `address` in Stücken wie das Gerät selbst (Grenzen `cuts`, max. 256 B), Pause `gapMs`.
func writeBlock(_ address: Int, _ data: [UInt8], cuts: [Int] = [], gapMs: Double = 20) {
    var bounds = Set(cuts.filter { $0 > 0 && $0 < data.count })
    var pos = 0
    while pos < data.count {
        var end = min(pos + 256, data.count)
        if let c = bounds.filter({ $0 > pos && $0 < end }).min() { end = c }
        dt1(address + pos, Array(data[pos..<end]))
        bounds.remove(end)
        pos = end
        Thread.sleep(forTimeInterval: gapMs / 1000)
    }
}

let tempPatch = 0x03 << 21
let patchSize = 486

// ── Befehle ──────────────────────────────────────────────────────────────────

let args = CommandLine.arguments.dropFirst()
switch args.first {
case "probe":
    let r = request(0x03 << 21, 16, expect: 16)
    guard let d = r.first else { print("keine Antwort"); exit(2) }
    print("Temp Patch: \"\(String(decoding: d.data, as: UTF8.self))\" (Prüfsumme \(d.sumOK ? "ok" : "FALSCH"))")

case "rq":
    let a = Array(args.dropFirst())
    guard a.count == 2, let adr = Int(a[0], radix: 16), let size = Int(a[1]) else { print("rq <addr-hex> <größe>"); exit(1) }
    // Eingabe wird als 4 Adressbytes gelesen (z. B. 03000001), nicht als lineare Zahl
    let bytes = (0..<4).map { UInt8((adr >> (8 * (3 - $0))) & 0xFF) }
    let r = request(addr7(bytes), size, quiet: 1.0)
    if r.isEmpty { print("keine Antwort") }
    for d in r {
        print("DT1 @ \(hex(be7(d.addr)))  \(d.data.count) Bytes  Prüfsumme \(d.sumOK ? "ok" : "FALSCH")")
        print("  \(hex(d.data))")
    }
    print("Summe: \(r.reduce(0) { $0 + $1.data.count }) Bytes in \(r.count) Nachrichten")

case "backup":
    guard let dir = args.dropFirst().first else { print("backup <verzeichnis>"); exit(1) }
    try FileManager.default.createDirectory(atPath: dir, withIntermediateDirectories: true)
    var log: [String] = []
    func save(_ file: String, _ label: String, _ address: Int, _ size: Int) -> [DT1] {
        let r = request(address, size, expect: size)
        let n = r.reduce(0) { $0 + $1.data.count }
        let bad = r.filter { !$0.sumOK }.count
        let line = "\(label.padding(toLength: 22, withPad: " ", startingAt: 0)) @ \(hex(be7(address)))  \(n)/\(size) Bytes  \(r.count) Msg  " +
                   (bad > 0 ? "\(bad) PRÜFSUMMENFEHLER" : (n == size ? "ok" : "UNVOLLSTÄNDIG"))
        print(line); log.append(line)
        if !r.isEmpty { FileManager.default.createFile(atPath: "\(dir)/\(file)", contents: Data(r.flatMap(\.raw))) }
        return r
    }
    let sizes = (patch: 486, system: Int(env["JD_SYSTEM_SIZE"] ?? "") ?? 0, perf: Int(env["JD_PERF_SIZE"] ?? "") ?? 0)
    if sizes.system > 0 { _ = save("system.syx", "System", 0x00 << 21, sizes.system) }
    var all: [UInt8] = []
    for n in 0..<64 {
        let r = save(String(format: "patch-I%d%d.syx", n / 8 + 1, n % 8 + 1),
                     String(format: "Patch I:%d%d", n / 8 + 1, n % 8 + 1), (0x06 << 21) | (n << 14), sizes.patch)
        all += r.flatMap(\.raw)
        if let first = r.first { log[log.count - 1] += "  \"\(String(decoding: first.data.prefix(16), as: UTF8.self))\"" }
    }
    FileManager.default.createFile(atPath: "\(dir)/patches-internal.syx", contents: Data(all))
    if sizes.perf > 0 {
        for n in 0..<16 { _ = save(String(format: "performance-%02d.syx", n + 1), "Performance \(n + 1)", (0x05 << 21) | (n << 14), sizes.perf) }
    }
    try log.joined(separator: "\n").appending("\n").write(toFile: "\(dir)/backup.log", atomically: true, encoding: .utf8)

case "save-temp":
    // Temp Patch lesen und als .syx sichern (Rohnachrichten des Geräts)
    guard let file = args.dropFirst().first else { print("save-temp <datei.syx>"); exit(1) }
    let r = request(tempPatch, patchSize, expect: patchSize)
    let n = r.reduce(0) { $0 + $1.data.count }
    guard n == patchSize, r.allSatisfy(\.sumOK) else { print("FEHLER: \(n)/\(patchSize) Bytes, Prüfsummen \(r.map(\.sumOK))"); exit(2) }
    FileManager.default.createFile(atPath: file, contents: Data(r.flatMap(\.raw)))
    print("Temp Patch \"\(String(decoding: r[0].data.prefix(16), as: UTF8.self))\" → \(file) (\(r.count) Msg, \(n) Byte)")

case "write-temp":
    // Patch aus .syx (beliebiger Bereich, z. B. 06 nn = Internal) in den Temp Patch schreiben
    // und byteweise zurücklesen. Nur der Offset im Patch (untere 14 Bit) zählt; Ziel ist immer 03 00 xx xx.
    let a = Array(args.dropFirst())
    guard let file = a.first, let raw = FileManager.default.contents(atPath: file) else { print("write-temp <datei.syx> [gap-ms]"); exit(1) }
    let gap = a.count > 1 ? Double(a[1]) ?? 20 : 20
    let msgs = splitSysex([UInt8](raw)).compactMap(parseDT1).sorted { $0.addr & 0x3FFF < $1.addr & 0x3FFF }
    var data: [UInt8] = []
    for m in msgs {
        guard m.sumOK, m.addr & 0x3FFF == data.count else { print("FEHLER: Datei nicht zusammenhängend bei \(hex(be7(m.addr)))"); exit(2) }
        data += m.data
    }
    guard data.count == patchSize else { print("FEHLER: \(data.count) statt \(patchSize) Byte"); exit(2) }
    writeBlock(tempPatch, data, cuts: [0x76], gapMs: gap)
    Thread.sleep(forTimeInterval: 0.05)
    guard let back = readBlock(tempPatch, patchSize) else { print("FEHLER: Zurücklesen gescheitert"); exit(3) }
    let diff = (0..<patchSize).filter { back[$0] != data[$0] }
    print("\"\(String(decoding: data.prefix(16), as: UTF8.self))\" → Temp, Rücklesen: " +
          (diff.isEmpty ? "BYTEIDENTISCH (\(patchSize) Byte)" : "\(diff.count) ABWEICHUNGEN an Offsets \(diff.prefix(20).map { String(format: "0x%03X", $0) })"))
    exit(diff.isEmpty ? 0 : 4)

case "name":
    // Arbeitsblatt B3: 16 Byte Name in den Temp Patch schreiben und zurücklesen
    let text = (args.dropFirst().first ?? "EDITOR TEST").padding(toLength: 16, withPad: " ", startingAt: 0)
    let bytes = Array(text.utf8.prefix(16))
    dt1(tempPatch, bytes)
    Thread.sleep(forTimeInterval: 0.05)
    let back = readBlock(tempPatch, 16) ?? []
    print("geschrieben \"\(text)\" · gelesen \"\(String(decoding: back, as: UTF8.self))\" · \(back == bytes ? "OK" : "ABWEICHUNG")")

case "timing":
    // Arbeitsblatt C2: 16 Einzelbyte-DT1 auf die Namensbytes mit fallendem Abstand; zählen, wie viele ankommen.
    guard let orig = readBlock(tempPatch, 16) else { print("keine Antwort"); exit(2) }
    for gap in [20.0, 10, 5, 3, 2, 1, 0] {
        for round in 0..<3 {
            let pattern = (0..<16).map { UInt8(0x41 + ($0 + round * 5 + Int(gap)) % 26) }   // A–Z
            for i in 0..<16 { dt1(tempPatch + i, [pattern[i]]); if gap > 0 { Thread.sleep(forTimeInterval: gap / 1000) } }
            Thread.sleep(forTimeInterval: 0.1)
            let back = readBlock(tempPatch, 16) ?? []
            let ok = zip(back, pattern).filter { $0 == $1 }.count
            print(String(format: "Abstand %4.0f ms  Runde %d: %2d/16 übernommen  %@", gap, round + 1, ok, String(decoding: back, as: UTF8.self)))
        }
    }
    dt1(tempPatch, orig)
    Thread.sleep(forTimeInterval: 0.05)
    print("Name wiederhergestellt: \(readBlock(tempPatch, 16) == orig ? "OK" : "FEHLER")")

case "sweep":
    // HIL-Sweep (ADR-6): Parameterliste als TSV (id, offset, bytes, min, max; Offset im Patch).
    // Je Parameter: min, max, mitte, 2 Zufallswerte → DT1 → ganzen Temp Patch zurücklesen →
    // Zielwert prüfen UND alle anderen 485 Bytes auf Kollateraländerungen prüfen.
    let a = Array(args.dropFirst())
    guard let file = a.first, let tsv = try? String(contentsOfFile: file, encoding: .utf8) else { print("sweep <params.tsv> [filter]"); exit(1) }
    let filter = a.count > 1 ? a[1] : ""
    guard var base = readBlock(tempPatch, patchSize) else { print("keine Antwort"); exit(2) }
    let original = base
    var rng = SystemRandomNumberGenerator()
    var seed: UInt64 = 0x4A44393930    // deterministisch: "JD990"
    func rnd(_ lo: Int, _ hi: Int) -> Int { seed = seed &* 6364136223846793005 &+ 1442695040888963407; return lo + Int((seed >> 33) % UInt64(hi - lo + 1)) }
    _ = rng
    var nParams = 0, nWrites = 0, nBad = 0
    let t0 = Date()
    for line in tsv.split(separator: "\n") where !line.hasPrefix("#") {
        let f = line.split(separator: "\t").map(String.init)
        guard f.count == 5, let off = Int(f[1]), let nb = Int(f[2]), let lo = Int(f[3]), let hi = Int(f[4]) else { continue }
        if !filter.isEmpty && !f[0].contains(filter) { continue }
        nParams += 1
        let cur = nb == 2 ? Int(base[off]) * 128 + Int(base[off + 1]) : Int(base[off])
        var values = [lo, hi, (lo + hi) / 2, rnd(lo, hi), rnd(lo, hi)]
        values = values.reduce(into: [Int]()) { if !$0.contains($1) { $0.append($1) } }
        var problems: [String] = []
        for v in values + [cur] {       // am Ende den Ausgangswert zurückschreiben
            let bytes: [UInt8] = nb == 2 ? [UInt8(v >> 7), UInt8(v & 0x7F)] : [UInt8(v)]
            dt1(tempPatch + off, bytes)   // 2-Byte-Parameter: immer ab MSB (LSB ist eine „#“-Adresse)
            nWrites += 1
            Thread.sleep(forTimeInterval: 0.03)
            guard let back = readBlock(tempPatch, patchSize) else { problems.append("v=\(v): Rücklesen gescheitert"); continue }
            var expect = base
            for (i, b) in bytes.enumerated() { expect[off + i] = b }
            let got = nb == 2 ? Int(back[off]) * 128 + Int(back[off + 1]) : Int(back[off])
            if got != v { problems.append("v=\(v) → gelesen \(got)") }
            let collateral = (0..<patchSize).filter { !(off..<(off + nb)).contains($0) && back[$0] != expect[$0] }
            if !collateral.isEmpty {
                problems.append("v=\(v): KOLLATERAL an " + collateral.prefix(8).map { String(format: "0x%03X: %d→%d", $0, expect[$0], back[$0]) }.joined(separator: ", "))
                for i in collateral { base[i] = back[i] }   // neue Basis, damit ein Folgefehler nicht alles rot färbt
            }
        }
        if !problems.isEmpty { nBad += 1 }
        print("\(problems.isEmpty ? "ok  " : "FEHL") \(f[0].padding(toLength: 28, withPad: " ", startingAt: 0)) @\(String(format: "0x%03X", off)) \(values) \(problems.joined(separator: " | "))")
        fflush(stdout)
    }
    // Ausgangszustand exakt wiederherstellen und bestätigen
    writeBlock(tempPatch, original, cuts: [0x76])
    Thread.sleep(forTimeInterval: 0.05)
    let restored = readBlock(tempPatch, patchSize) == original
    print(String(format: "\n%d Parameter, %d Schreibvorgänge, %d mit Befund, %.0f s · Ausgangszustand wiederhergestellt: %@",
                 nParams, nWrites, nBad, Date().timeIntervalSince(t0), restored ? "JA (byteidentisch)" : "NEIN ⚠️"))
    exit(nBad == 0 && restored ? 0 : 5)

case "listen":
    // Passiv mitschneiden, was das Gerät von sich aus sendet (M0-Frage 3). Sendet nichts.
    let secs = Double(args.dropFirst().first ?? "") ?? 60
    var raw: [UInt8] = []
    var rawIn = MIDIPortRef()
    let t0 = Date()
    MIDIInputPortCreateWithBlock(client, "listen" as CFString, &rawIn) { list, _ in
        for p in list.unsafeSequence() {
            let bytes = withUnsafeBytes(of: p.pointee.data) { Array($0.prefix(Int(p.pointee.length))) }
            lock.lock(); defer { lock.unlock() }
            for b in bytes where b != 0xF8 && b != 0xFE {   // Clock / Active Sensing ausblenden
                if b & 0x80 != 0, b != 0xF7, !raw.isEmpty { print(String(format: "%7.2fs  %@", Date().timeIntervalSince(t0), hex(raw))); raw = [] }
                raw.append(b)
                if b == 0xF7 {
                    let line = String(format: "%7.2fs  %@", Date().timeIntervalSince(t0), hex(raw))
                    if let d = parseDT1(raw) { print("\(line)\n          → DT1 @ \(hex(be7(d.addr))), \(d.data.count) Byte, Prüfsumme \(d.sumOK ? "ok" : "FALSCH")") }
                    else { print(line) }
                    raw = []
                }
            }
            fflush(stdout)
        }
    }
    MIDIPortConnectSource(rawIn, src, nil)
    print("Lausche \(Int(secs)) s auf \(portName) …"); fflush(stdout)
    Thread.sleep(forTimeInterval: secs)
    lock.lock(); if !raw.isEmpty { print(hex(raw)) }; lock.unlock()
    print("Ende.")

default:
    print("probe | rq <addr-hex> <größe> | backup <verzeichnis> | listen [sekunden]")
}
