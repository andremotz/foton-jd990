//! `jd990` — Werkzeug für JD-990-Patches.
//!
//! Offline:
//!   jd990 show <datei.syx>            Alle Parameter mit Rohwert und Anzeige
//!   jd990 verify <datei>…             .syx/.mid, Einzelpatch oder Bank: Bereichsprüfung
//!   jd990 init <out.syx> [name=…] [id[:Tone]=wert…]  Init-Patch (optional geändert) als .syx — z. B. Test-Fixtures
//!   jd990 convert <in> <out.syx>      .syx/.mid → .syx in Gerätestückelung; JD-800 → JD-990 konvertiert
//!   jd990 diff <a.syx> <b.syx>        Unterschiedliche Parameter zweier Patches
//!
//! Am Gerät (Port: JD_PORT, Default „Unitor8/AMT8 Port 2“; Device: JD_DEV, Default 16):
//!   jd990 ports                       MIDI-Ausgänge auflisten
//!   jd990 probe                       Name des Temp Patch — antwortet das Gerät?
//!   jd990 dump temp|I11…I88 <out.syx> Patch lesen und speichern
//!   jd990 backup <verzeichnis>        System + 64 Patches + 16 Performances (nur lesen)
//!   jd990 send-temp <datei.syx>       Patch in den Temp-Bereich, mit Rücklese-Prüfung
//!   jd990 set <param-id> [A|B|C|D] <rohwert>   Einzelparameter im Temp Patch, geprüft
//!   jd990 poke <offset-hex> <byte>…  Diagnose: Rohbytes als EIN DT1 in den Temp Patch
//!   jd990 hil-sweep [filter] [--full] [--part=1…7]  ADR-6: jeden Parameter setzen, zurücklesen, Nachbarn prüfen
//!                                     (--part: im Temp-Patch eines Performance-Parts, 02 0n)
//!
//! Performance-Modus (ADR-22; Schreiben nur Temp 01/02 0n und System mode):
//!   jd990 mode [patch|performance]    System-Modus lesen bzw. umschalten (misst die Zeit)
//!   jd990 dump-setup <out.syx>        Temp-Performance + Part-Patches 1–7 lesen und speichern
//!   jd990 send-setup <datei.syx>      Setup in die Temp-Bereiche, mit Rücklese-Prüfung
//!   jd990 send-perf <datei.syx>       nur die Performance (ein DT1, 144 Byte) — zeigt, ob Part-Patches neu geladen werden
//!   jd990 perf-set <id> [1…8] <rohwert>  Performance-/Part-Parameter, geprüft (zeigt, ob sich Part-Patches ändern)
//!   jd990 perf-sweep [filter]         HIL über Common + Parts 1–7, Nachbarn prüfen, danach Ausgangszustand
//!
//! Bibliothek (M5; Ort: JD990_LIBRARY, sonst ~/Library/Application Support/JD-990 Editor/):
//!   jd990 lib import <datei>…         .syx/.mid (Einzelpatch oder Bank), Dubletten werden erkannt
//!   jd990 lib list [suche] [--fav]    Einträge (Suche in Name, Quelle, Tags, Notizen)
//!   jd990 lib show <id>               Eintrag mit allen Parametern
//!   jd990 lib diff <id> <id|datei>    unterschiedliche Parameter
//!   jd990 lib tag <id> <tag,…> · fav <id> on|off · note <id> <text> · rm <id>
//!   jd990 lib export <id>… <out.syx>  ein Eintrag → Temp-Patch, mehrere → Bank I:11 …
//!   jd990 lib send <id>               in den Temp-Bereich des Geräts, mit Rücklese-Prüfung
//!                                     (--full: jeden Rohwert; JD_SWEEP_MAX_S = Zeitgrenze in s)

use jd990_core::params::{PATCH_PARAMS, ParamDef, TONE_PARAMS};
use jd990_core::{Patch, Tone};
use jd990_core::sysex::Address;
use jd990_midi::Device;
use std::process::ExitCode;

fn load(path: &str) -> Result<(Patch, jd990_core::sysex::Address, Vec<u8>), String> {
    let bytes = std::fs::read(path).map_err(|e| format!("{path}: {e}"))?;
    let (p, base) = Patch::from_syx(&bytes).map_err(|e| format!("{path}: {e}"))?;
    Ok((p, base, bytes))
}

fn shown(def: &ParamDef, raw: u16) -> String {
    jd990_core::display::format(def, raw)
}

fn all(p: &Patch) -> Vec<(String, &'static ParamDef, u16)> {
    let mut out: Vec<_> = PATCH_PARAMS.iter().map(|d| (d.id.to_string(), d, p.get(d, None))).collect();
    for t in Tone::ALL {
        for d in TONE_PARAMS {
            out.push((d.id.replacen("tone.", &format!("tone{}.", t.letter()), 1), d, p.get(d, Some(t))));
        }
    }
    out
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match args.first().map(String::as_str) {
        Some("show") if args.len() == 2 => match load(&args[1]) {
            Ok((p, base, _)) => {
                println!("\"{}\"  @ {base}", p.name());
                let mut group = "";
                for (id, d, v) in all(&p) {
                    if d.group != group {
                        group = d.group;
                        println!("── {group}");
                    }
                    let tone = id.strip_prefix("tone").and_then(|r| r.chars().next()).and_then(|c| Tone::ALL.into_iter().find(|t| t.letter() == c));
                    println!("  {id:30} {v:>4}  {}", jd990_core::display::format_in(&p, d, tone));
                }
                ExitCode::SUCCESS
            }
            Err(e) => fail(&e),
        },
        Some("verify") if args.len() >= 2 => {
            let (mut files, mut patches, mut bad) = (0, 0, 0);
            for path in &args[1..] {
                files += 1;
                let bytes = match std::fs::read(path) {
                    Ok(b) => b,
                    Err(e) => {
                        bad += 1;
                        println!("FEHL {path}: {e}");
                        continue;
                    }
                };
                match Patch::all_from_bytes(&bytes) {
                    Ok(all) if all.is_empty() => println!("--   {path}: keine Patch-Daten"),
                    Ok(all) => {
                        // Byteidentität nur prüfbar, wenn die Datei ausschließlich Patch-DT1 in
                        // Gerätestückelung enthält (eigene Dumps); fremde Dateien stückeln anders.
                        let again: Vec<u8> = all.iter().flat_map(|(p, a)| p.to_syx(0x10, *a)).collect();
                        let identical = again == bytes;
                        for (p, at) in &all {
                            patches += 1;
                            let oor = p.out_of_range();
                            bad += !oor.is_empty() as usize;
                            if !oor.is_empty() || all.len() == 1 {
                                println!(
                                    "{} {path} @ {at}  \"{}\"{}",
                                    if oor.is_empty() { "ok  " } else { "FEHL" },
                                    p.name(),
                                    if oor.is_empty() { String::new() } else { format!("  {} Werte außerhalb: {}", oor.len(), oor.iter().take(3).map(|(d, t, v)| format!("{}{} = {v}", d.id, t.map(|t| format!("[{}]", t.letter())).unwrap_or_default())).collect::<Vec<_>>().join(", ")) }
                                );
                            }
                        }
                        if all.len() > 1 {
                            println!("ok   {path}: {} Patches", all.len());
                        }
                        if !identical && !jd990_core::smf::is_smf(&bytes) {
                            println!("     {path}: Neu-Kodierung nicht byteidentisch (andere Stückelung oder Fremddaten in der Datei)");
                        }
                    }
                    Err(e) => {
                        bad += 1;
                        println!("FEHL {path}: {e}");
                    }
                }
            }
            println!("{files} Dateien, {patches} Patches, {bad} mit Befund");
            if bad == 0 { ExitCode::SUCCESS } else { ExitCode::FAILURE }
        }
        Some("init") if args.len() >= 2 => {
            // Init-Patch als .syx, optional mit Änderungen: tone.tvf.cutoff:A=40 patch.level=90
            let mut p = Patch::init();
            for a in &args[2..] {
                if let Some(name) = a.strip_prefix("name=") {
                    let mut raw = *p.as_bytes();
                    let n = format!("{name:<16}");
                    raw[..16].copy_from_slice(&n.as_bytes()[..16]);
                    p = Patch::from_bytes(raw);
                    continue;
                }
                let Some((lhs, v)) = a.split_once('=') else { return fail(&format!("{a}: erwartet id[:Tone]=wert")) };
                let (id, tone) = match lhs.split_once(':') {
                    Some((i, t)) => (i, Tone::ALL.into_iter().find(|x| x.letter().to_string() == t.to_uppercase())),
                    None => (lhs, None),
                };
                let Some(def) = jd990_core::params::find(id) else { return fail(&format!("{id}: unbekannter Parameter")) };
                let Ok(v) = v.parse::<u16>() else { return fail(&format!("{a}: Wert keine Zahl")) };
                if let Err(e) = p.set(def, tone, v) { return fail(&e.to_string()); }
            }
            match std::fs::write(&args[1], p.to_syx(0x10, Address::TEMP_PATCH)) {
                Ok(()) => { println!("Init-Patch → {}", args[1]); ExitCode::SUCCESS }
                Err(e) => fail(&e.to_string()),
            }
        }
        Some("convert") if args.len() == 3 => {
            // .syx/.mid → .syx in Gerätestückelung, gleiche Adressen. JD-800-Patches werden
            // konvertiert: einer → Temp-Patch, mehrere → Bank I:11 …
            let result = std::fs::read(&args[1]).map_err(|e| e.to_string()).and_then(|b| {
                let mut all = Patch::all_from_bytes(&b).map_err(|e| e.to_string())?;
                let jd800 = jd990_core::jd800::patches_from_bytes(&b);
                if !jd800.is_empty() {
                    let n = jd800.len();
                    for (i, (p8, label)) in jd800.into_iter().enumerate() {
                        eprintln!("JD-800 {label} → JD-990");
                        let at = if n == 1 { Address::TEMP_PATCH } else { Address::internal_patch(i as u8 % 64) };
                        all.push((jd990_core::jd800::convert(&p8), at));
                    }
                }
                Ok(all)
            });
            match result {
                Ok(all) => {
                    let out: Vec<u8> = all.iter().flat_map(|(p, a)| p.to_syx(0x10, *a)).collect();
                    match std::fs::write(&args[2], out) {
                        Ok(()) => {
                            println!("{} Patches → {}", all.len(), args[2]);
                            ExitCode::SUCCESS
                        }
                        Err(e) => fail(&e.to_string()),
                    }
                }
                Err(e) => fail(&e),
            }
        }
        Some("diff") if args.len() == 3 => match (load(&args[1]), load(&args[2])) {
            (Ok((a, ..)), Ok((b, ..))) => {
                println!("\"{}\" ↔ \"{}\"", a.name(), b.name());
                if a.name() != b.name() {
                    println!("  {:30} \"{}\" → \"{}\"", "patch.name", a.name(), b.name());
                }
                let n = all(&a).into_iter().zip(all(&b)).filter(|((_, _, x), (_, _, y))| x != y).inspect(|((id, d, x), (_, _, y))| {
                    println!("  {id:30} {x:>4} → {y:<4} {}", shown(d, *y));
                }).count();
                println!("{n} Parameter unterschiedlich");
                ExitCode::SUCCESS
            }
            (Err(e), _) | (_, Err(e)) => fail(&e),
        },
        Some("lib") => match library(&args[1..]) {
            Ok(()) => ExitCode::SUCCESS,
            Err(e) => fail(&e.to_string()),
        },
        Some(cmd @ ("ports" | "probe" | "dump" | "backup" | "send-temp" | "set" | "poke" | "hil-sweep" | "mode" | "dump-setup" | "send-setup" | "send-perf" | "perf-set" | "perf-sweep")) => match hardware(cmd, &args[1..]) {
            Ok(()) => ExitCode::SUCCESS,
            Err(e) => fail(&e.to_string()),
        },
        _ => {
            eprintln!("jd990 show <datei.syx> | verify <datei>… | convert <in> <out.syx> | diff <a.syx> <b.syx>");
            ExitCode::from(2)
        }
    }
}

fn fail(e: &str) -> ExitCode {
    eprintln!("Fehler: {e}");
    ExitCode::FAILURE
}

fn open() -> Result<Device, jd990_midi::Error> {
    let port = std::env::var("JD_PORT").unwrap_or_else(|_| jd990_midi::DEFAULT_PORT.into());
    let dev = std::env::var("JD_DEV").ok().and_then(|d| d.parse().ok()).unwrap_or(0x10);
    let mut d = Device::open(&port, dev)?;
    if let Some(ms) = std::env::var("JD_GAP_MS").ok().and_then(|g| g.parse().ok()) {
        d.dt1_gap = std::time::Duration::from_millis(ms);
    }
    Ok(d)
}

/// „I11“ … „I88“ → Patchnummer 0–63
fn patch_number(s: &str) -> Option<u8> {
    let b = s.strip_prefix('I').or_else(|| s.strip_prefix("I-"))?.as_bytes();
    let (bank, num) = (b.first()?.wrapping_sub(b'1'), b.get(1)?.wrapping_sub(b'1'));
    (b.len() == 2 && bank < 8 && num < 8).then_some(bank * 8 + num)
}

fn hardware(cmd: &str, args: &[String]) -> Result<(), Box<dyn std::error::Error>> {
    match (cmd, args) {
        ("ports", []) => {
            for p in jd990_midi::output_ports() {
                println!("{p}");
            }
        }
        ("probe", []) => println!("Temp Patch: \"{}\"", open()?.probe()?),
        ("dump", [which, out]) => {
            let at = if which == "temp" { Address::TEMP_PATCH } else { Address::internal_patch(patch_number(which).ok_or("Patch: temp oder I11…I88")?) };
            let p = open()?.read_patch(at)?;
            std::fs::write(out, p.to_syx(0x10, at))?;
            println!("\"{}\" @ {at} → {out}", p.name());
        }
        ("backup", [dir]) => {
            let mut d = open()?;
            std::fs::create_dir_all(dir)?;
            let block = |d: &mut Device, at: Address, size: usize| -> Result<Vec<u8>, jd990_midi::Error> {
                let data = d.read(at, size)?;
                // wie vom Gerät gesendet: ≤ 256 Byte je DT1
                Ok(data.chunks(256).enumerate().flat_map(|(i, c)| jd990_core::sysex::Dt1 { device: d.device_id, address: at.offset(i * 256), data: c.to_vec() }.encode()).collect())
            };
            std::fs::write(format!("{dir}/system.syx"), block(&mut d, Address(0), 38)?)?;
            let mut bank = Vec::new();
            for n in 0..64u8 {
                let at = Address::internal_patch(n);
                let p = d.read_patch(at)?;
                let syx = p.to_syx(d.device_id, at);
                std::fs::write(format!("{dir}/patch-I{}{}.syx", n / 8 + 1, n % 8 + 1), &syx)?;
                bank.extend(syx);
                eprint!("\r{:2}/64 \"{}\"", n + 1, p.name());
            }
            std::fs::write(format!("{dir}/patches-internal.syx"), bank)?;
            for n in 0..16u32 {
                std::fs::write(format!("{dir}/performance-{:02}.syx", n + 1), block(&mut d, Address(0x05 << 21 | n << 14), 144)?)?;
            }
            eprintln!();
            println!("Backup vollständig: System + 64 Patches + 16 Performances → {dir}");
        }
        ("send-temp", [file]) => {
            let (p, _) = Patch::from_syx(&std::fs::read(file)?)?;
            open()?.write_temp_patch(&p)?;
            println!("\"{}\" → Temp Patch, Rücklesen byteidentisch", p.name());
        }
        ("set", rest) if rest.len() == 2 || rest.len() == 3 => {
            let def = jd990_core::params::find(&rest[0]).ok_or("unbekannter Parameter")?;
            let tone = if rest.len() == 3 {
                Some(match rest[1].as_str() { "A" => Tone::A, "B" => Tone::B, "C" => Tone::C, "D" => Tone::D, _ => return Err("Tone: A|B|C|D".into()) })
            } else {
                None
            };
            if tone.is_some() != def.id.starts_with("tone.") {
                return Err("Tone-Parameter brauchen A|B|C|D, Patch-Parameter nicht".into());
            }
            let value: u16 = rest.last().unwrap().parse()?;
            let mut d = open()?;
            let mut p = d.read_patch(Address::TEMP_PATCH)?;
            let before = p.clone();
            let (offset, bytes) = p.set(def, tone, value)?;
            d.write_temp(Address::TEMP_PATCH.offset(offset), &bytes)?;
            let back = d.read_patch(Address::TEMP_PATCH)?;
            let got = back.get(def, tone);
            let collateral: Vec<_> = (0..jd990_core::params::PATCH_SIZE)
                .filter(|&i| !(offset..offset + bytes.len()).contains(&i) && back.as_bytes()[i] != before.as_bytes()[i])
                .collect();
            println!("{} = {got} {} {}", def.id, shown(def, got), if got == value { "✓" } else { "✗ nicht übernommen" });
            if !collateral.is_empty() {
                println!("  Gerät hat außerdem geändert: {collateral:?}");
            }
        }
        ("poke", [off, bytes @ ..]) if !bytes.is_empty() => {
            let off = usize::from_str_radix(off.trim_start_matches("0x"), 16)?;
            let data = bytes.iter().map(|b| b.parse::<u8>()).collect::<Result<Vec<_>, _>>()?;
            if off + data.len() > jd990_core::params::PATCH_SIZE || data.iter().any(|&b| b > 0x7F) {
                return Err("außerhalb des Patches oder Byte > 127".into());
            }
            let mut d = open()?;
            d.write_temp(Address::TEMP_PATCH.offset(off), &data)?;
            let back = d.read_patch(Address::TEMP_PATCH)?;
            let got = &back.as_bytes()[off..off + data.len()];
            println!("geschrieben {data:?} ab {off:#05x} · gelesen {got:?} {}", if got == data.as_slice() { "✓" } else { "✗" });
        }
        ("hil-sweep", rest) if rest.len() <= 3 => {
            let full = rest.iter().any(|a| a == "--full");
            let part = rest.iter().find_map(|a| a.strip_prefix("--part=")).map(|n| n.parse::<u8>()).transpose()?;
            let base = match part {
                Some(n @ 1..=7) => Address::perf_patch(n - 1),
                Some(_) => return Err("--part=1…7".into()),
                None => Address::TEMP_PATCH,
            };
            let filter = rest.iter().find(|a| *a != "--full" && !a.starts_with("--part=")).map(String::as_str).unwrap_or("");
            hil_sweep(filter, full, base)?
        }
        ("mode", []) => {
            let m = open()?.mode()?;
            println!("System-Modus: {}", ["PATCH", "PERFORMANCE", "RHYTHM"].get(m as usize).unwrap_or(&"?"));
        }
        ("mode", [which]) => {
            let m = match which.as_str() { "patch" => 0, "performance" => 1, _ => return Err("patch|performance".into()) };
            let t = open()?.set_mode(m)?;
            println!("System-Modus → {which}, bestätigt nach {} ms", t.as_millis());
        }
        ("dump-setup", [out]) => {
            let mut d = open()?;
            let perf = d.read_performance(Address::TEMP_PERFORMANCE)?;
            let patches: Vec<Patch> = (0..7u8).map(|n| d.read_patch(Address::perf_patch(n))).collect::<Result<_, _>>()?;
            let setup = jd990_core::Setup { performance: perf, patches: patches.try_into().unwrap() };
            std::fs::write(out, setup.to_syx(d.device_id))?;
            print_setup(&setup);
            println!("→ {out}");
        }
        ("send-setup", [file]) => {
            let setup = jd990_core::Setup::from_bytes(&std::fs::read(file)?)?;
            let mut d = open()?;
            d.write_performance(&setup.performance)?;
            for (n, p) in setup.patches.iter().enumerate() {
                d.write_patch_at(Address::perf_patch(n as u8), p)?;
            }
            print_setup(&setup);
            println!("→ Temp-Performance + Part-Patches 1–7, Rücklesen byteidentisch");
        }
        ("send-perf", [file]) => {
            let setup_perf = jd990_core::Performance::all_from_bytes(&std::fs::read(file)?)?.into_iter().next().ok_or("keine Performance in der Datei")?.0;
            let mut d = open()?;
            let before: Vec<Patch> = (0..7u8).map(|n| d.read_patch(Address::perf_patch(n))).collect::<Result<_, _>>()?;
            d.write_performance(&setup_perf)?;
            std::thread::sleep(std::time::Duration::from_millis(500));
            for (n, b) in before.iter().enumerate() {
                let a = d.read_patch(Address::perf_patch(n as u8))?;
                println!("  Part {} {}: \"{}\" → \"{}\"", n + 1, if a == *b { "unverändert" } else { "NEU GELADEN" }, b.name().trim_end(), a.name().trim_end());
            }
        }
        ("perf-set", rest) if rest.len() == 2 || rest.len() == 3 => {
            let def = jd990_core::params::find_perf(&rest[0]).ok_or("unbekannter Performance-Parameter")?;
            let part = if rest.len() == 3 { Some(rest[1].parse::<usize>()?.checked_sub(1).filter(|&p| p < 8).ok_or("Part 1…8")?) } else { None };
            if part.is_some() != def.id.starts_with("part.") {
                return Err("Part-Parameter brauchen 1…8, Common-Parameter nicht".into());
            }
            let value: u16 = rest.last().unwrap().parse()?;
            let mut d = open()?;
            let mut perf = d.read_performance(Address::TEMP_PERFORMANCE)?;
            let before = perf.clone();
            let patch_before = match part { Some(p) if p < 7 => Some(d.read_patch(Address::perf_patch(p as u8))?), _ => None };
            let (offset, bytes) = perf.set(def, part, value)?;
            let t0 = std::time::Instant::now();
            d.write(Address::TEMP_PERFORMANCE.offset(offset), &bytes)?;
            std::thread::sleep(std::time::Duration::from_millis(150));
            let back = d.read_performance(Address::TEMP_PERFORMANCE)?;
            let got = back.get(def, part);
            let collateral: Vec<usize> = (0..jd990_core::params::PERF_SIZE)
                .filter(|&i| !(offset..offset + bytes.len()).contains(&i) && back.as_bytes()[i] != before.as_bytes()[i])
                .collect();
            println!("{}{} = {got} {} {} ({} ms)", def.id, part.map(|p| format!("[{}]", p + 1)).unwrap_or_default(), back.display(def, part),
                     if got == perf.get(def, part) { "✓" } else { "✗ nicht übernommen" }, t0.elapsed().as_millis());
            if !collateral.is_empty() {
                println!("  Gerät hat in der Performance außerdem geändert: {collateral:?}");
            }
            if let (Some(p), Some(pb)) = (part, patch_before) {
                let pa = d.read_patch(Address::perf_patch(p as u8))?;
                if pa != pb {
                    println!("  Part-Patch {} hat sich geändert: \"{}\" → \"{}\"", p + 1, pb.name().trim_end(), pa.name().trim_end());
                } else {
                    println!("  Part-Patch {} unverändert (\"{}\")", p + 1, pa.name().trim_end());
                }
            }
        }
        ("perf-sweep", rest) if rest.len() <= 1 => perf_sweep(rest.first().map(String::as_str).unwrap_or(""))?,
        _ => return Err(format!("falsche Argumente für {cmd}").into()),
    }
    Ok(())
}

/// ADR-6: Hardware-in-the-Loop-Sweep über die komplette Spec. Enums und kleine
/// Bereiche (≤ 32 Werte) vollständig, sonst Min/Max/Mitte + 2 Zufallswerte.
/// Erwartet werden die Geräteregeln aus `jd990_core::rules`; alles andere ist ein Befund.
fn print_setup(s: &jd990_core::Setup) {
    let p = &s.performance;
    let find = jd990_core::params::find_perf;
    println!("Performance \"{}\" · Stimmen reserviert {}/24", p.name().trim_end(), p.voices_reserved());
    for part in 0..8 {
        let on = p.get(find("part.rx_sw").unwrap(), Some(part)) == 1;
        let ch = p.get(find("part.rx_ch").unwrap(), Some(part)) + 1;
        let patch = if part < 7 { format!("\"{}\"", s.patches[part].name().trim_end()) } else { "(Rhythm)".into() };
        println!("  Part {} {} Kanal {ch:2} {:5} {patch:20} Level {:3} Reserve {}", part + 1, if on { "an " } else { "aus" }, p.patch_slot(part),
                 p.get(find("part.level").unwrap(), Some(part)), p.get(find(&format!("perf.reserve_{}", part + 1)).unwrap(), None));
    }
}

/// HIL über die Performance (Common + Parts 1–7): setzen, ganze Performance zurücklesen,
/// Nachbarn prüfen. Patchwahl (Bank/Patch) wird übersprungen — sie lädt Speicherpatches
/// in die Part-Patches; das prüft `perf-set` gezielt. Am Ende: Ausgangszustand.
fn perf_sweep(filter: &str) -> Result<(), Box<dyn std::error::Error>> {
    use jd990_core::params::{PART_PARAMS, PERF_PARAMS, PERF_SIZE};
    let mut d = open()?;
    let original = d.read_performance(Address::TEMP_PERFORMANCE)?;
    let patches: Vec<Patch> = (0..7u8).map(|n| d.read_patch(Address::perf_patch(n))).collect::<Result<_, _>>()?;
    let mut model = original.clone();
    let mut targets: Vec<(&'static ParamDef, Option<usize>)> = PERF_PARAMS.iter().map(|p| (p, None)).collect();
    for part in 0..7 {
        targets.extend(PART_PARAMS.iter().filter(|p| p.id != "part.bank" && p.id != "part.patch").map(|p| (p, Some(part))));
    }
    let start = std::time::Instant::now();
    let (mut n_params, mut n_writes, mut n_bad) = (0, 0, 0);
    for (def, part) in targets {
        let name = match part { Some(p) => format!("{}[{}]", def.id, p + 1), None => def.id.to_string() };
        if !name.contains(filter) {
            continue;
        }
        n_params += 1;
        let current = model.get(def, part);
        let mut values: Vec<u16> = if def.max - def.min < 32 { (def.min..=def.max).collect() } else { vec![def.min, def.max, (def.min + def.max) / 2] };
        values.push(current);
        let mut problems = Vec::new();
        for v in values {
            let mut expect = model.clone();
            let (offset, bytes) = expect.set(def, part, v)?;
            d.write(Address::TEMP_PERFORMANCE.offset(offset), &bytes)?;
            n_writes += 1;
            std::thread::sleep(std::time::Duration::from_millis(30));
            let back = d.read_performance(Address::TEMP_PERFORMANCE)?;
            let diff: Vec<usize> = (0..PERF_SIZE).filter(|&i| back.as_bytes()[i] != expect.as_bytes()[i]).collect();
            if !diff.is_empty() {
                problems.push(format!("v={v}: {}", diff.iter().take(6).map(|&i| format!("{i:#04x} erwartet {} gelesen {}", expect.as_bytes()[i], back.as_bytes()[i])).collect::<Vec<_>>().join(", ")));
            }
            model = back;
        }
        if !problems.is_empty() {
            n_bad += 1;
        }
        println!("{} {name:24} {}", if problems.is_empty() { "ok  " } else { "FEHL" }, problems.join(" | "));
    }
    d.write_performance(&original)?;
    let mut patches_changed = Vec::new();
    for (n, p) in patches.iter().enumerate() {
        if d.read_patch(Address::perf_patch(n as u8))? != *p {
            patches_changed.push(n + 1);
            d.write_patch_at(Address::perf_patch(n as u8), p)?;
        }
    }
    println!(
        "\n{n_params} Parameter, {n_writes} Schreibvorgänge, {n_bad} mit Befund, {:.0} s · Ausgangszustand wiederhergestellt (byteidentisch){}",
        start.elapsed().as_secs_f64(),
        if patches_changed.is_empty() { String::new() } else { format!(" · Part-Patches {patches_changed:?} hatten sich verändert und wurden zurückgeschrieben") }
    );
    if n_bad > 0 { Err(format!("{n_bad} Parameter mit Befund").into()) } else { Ok(()) }
}

fn hil_sweep(filter: &str, full: bool, base: Address) -> Result<(), Box<dyn std::error::Error>> {
    let max_s: u64 = std::env::var("JD_SWEEP_MAX_S").ok().and_then(|v| v.parse().ok()).unwrap_or(u64::MAX);
    use jd990_core::params::PATCH_SIZE;
    let mut d = open()?;
    let original = d.read_patch(base)?;
    let mut model = original.clone();
    let mut seed: u64 = 0x4A44393930;
    let mut rnd = |lo: u16, hi: u16| {
        seed = seed.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        lo + ((seed >> 33) % (hi - lo + 1) as u64) as u16
    };
    let mut targets: Vec<(&'static ParamDef, Option<Tone>)> = PATCH_PARAMS.iter().map(|p| (p, None)).collect();
    for t in Tone::ALL {
        targets.extend(TONE_PARAMS.iter().map(|p| (p, Some(t))));
    }
    let start = std::time::Instant::now();
    let (mut n_params, mut n_writes, mut n_bad) = (0, 0, 0);
    for (def, tone) in targets {
        let name = match tone { Some(t) => def.id.replacen("tone.", &format!("tone{}.", t.letter()), 1), None => def.id.to_string() };
        if !name.contains(filter) {
            continue;
        }
        let current = model.get(def, tone);
        if start.elapsed().as_secs() > max_s {
            println!("ZEITGRENZE erreicht ({max_s} s) — Abbruch vor {name}, Ausgangszustand wird wiederhergestellt");
            break;
        }
        n_params += 1; // erst nach der Zeitprüfung zählen
        let mut values: Vec<u16> = if full || def.max - def.min < 32 {
            (def.min..=def.max).collect()
        } else {
            vec![def.min, def.max, (def.min + def.max) / 2, rnd(def.min, def.max), rnd(def.min, def.max)]
        };
        values.dedup();
        values.push(current); // Ausgangswert zurück
        let mut problems = Vec::new();
        for v in values {
            let mut expect = model.clone();
            let (offset, bytes) = expect.set(def, tone, v)?;
            jd990_core::rules::apply_after_set(&mut expect, def, tone);
            d.write(base.offset(offset), &bytes)?;
            n_writes += 1;
            std::thread::sleep(std::time::Duration::from_millis(30));
            let back = d.read_patch(base)?;
            let diff: Vec<usize> = (0..PATCH_SIZE).filter(|&i| back.as_bytes()[i] != expect.as_bytes()[i]).collect();
            if !diff.is_empty() {
                problems.push(format!(
                    "v={v}: {}",
                    diff.iter().take(6).map(|&i| format!("{i:#05x} erwartet {} gelesen {}", expect.as_bytes()[i], back.as_bytes()[i])).collect::<Vec<_>>().join(", ")
                ));
            }
            model = back; // vom echten Gerätezustand aus weiter
        }
        if !problems.is_empty() {
            n_bad += 1;
        }
        println!("{} {name:30} {}", if problems.is_empty() { "ok  " } else { "FEHL" }, problems.join(" | "));
    }
    d.write_patch_at(base, &original)?;
    println!(
        "\n{n_params} Parameter, {n_writes} Schreibvorgänge, {n_bad} mit Befund, {:.0} s · Ausgangszustand wiederhergestellt (byteidentisch geprüft)",
        start.elapsed().as_secs_f64()
    );
    if n_bad > 0 { Err(format!("{n_bad} Parameter mit Befund").into()) } else { Ok(()) }
}

fn library(args: &[String]) -> Result<(), Box<dyn std::error::Error>> {
    use jd990_library::{Error as LibError, Library};
    let lib = Library::open(Library::default_path())?;
    let id = |s: &String| -> Result<i64, Box<dyn std::error::Error>> { Ok(s.parse::<i64>()?) };
    let a: Vec<&str> = args.iter().map(String::as_str).collect();
    match a.as_slice() {
        ["import", files @ ..] if !files.is_empty() => {
            for f in files {
                match lib.import_file(f) {
                    Ok(r) => println!("{f}: {} neu, {} schon vorhanden", r.added.len(), r.duplicates.len()),
                    Err(e) => println!("{f}: {e}"),
                }
            }
            println!("Bibliothek: {} Patches ({})", lib.count()?, Library::default_path().display());
        }
        ["list", rest @ ..] => {
            let fav = rest.contains(&"--fav");
            let query = rest.iter().find(|x| **x != "--fav").copied().unwrap_or("");
            for e in lib.list(query, fav)? {
                println!("{:5} {} {:16}  {:28} {}", e.id, if e.favorite { "★" } else { " " }, e.name, e.source, e.tags.join(", "));
            }
        }
        ["show", n] => {
            let n = id(&n.to_string())?;
            let p = lib.get(n)?;
            let e = lib.list("", false)?.into_iter().find(|e| e.id == n).ok_or(LibError::NotFound(n))?;
            println!("#{n} \"{}\" · {} · {}{}", e.name, e.source, e.added, if e.favorite { " · ★" } else { "" });
            if !e.tags.is_empty() { println!("Tags: {}", e.tags.join(", ")); }
            if !e.notes.is_empty() { println!("Notiz: {}", e.notes); }
            for (pid, d, v) in all(&p) {
                println!("  {pid:30} {v:>4}  {}", shown(d, v));
            }
        }
        ["diff", x, y] => {
            let a = lib.get(id(&x.to_string())?)?;
            let b = match y.parse::<i64>() {
                Ok(n) => lib.get(n)?,
                Err(_) => Patch::from_syx(&std::fs::read(y)?)?.0,
            };
            let d = jd990_library::diff(&a, &b);
            for (pid, va, vb) in &d { println!("  {pid:30} {va:>16} → {vb}"); }
            println!("{} Unterschiede", d.len());
        }
        ["tag", n, tags] => lib.set_tags(id(&n.to_string())?, &tags.split(',').collect::<Vec<_>>())?,
        ["fav", n, on] => lib.set_favorite(id(&n.to_string())?, *on == "on")?,
        ["note", n, text @ ..] => lib.set_notes(id(&n.to_string())?, &text.join(" "))?,
        ["rm", n] => lib.remove(id(&n.to_string())?)?,
        ["export", rest @ ..] if rest.len() >= 2 => {
            let (out, ids) = rest.split_last().unwrap();
            let ids = ids.iter().map(|s| s.parse::<i64>()).collect::<Result<Vec<_>, _>>()?;
            std::fs::write(out, lib.export_syx(&ids)?)?;
            println!("{} Patch(es) → {out}{}", ids.len(), if ids.len() > 1 { " (Bank, Plätze I:11 …)" } else { " (Temp-Patch)" });
        }
        ["send", n] => {
            let p = lib.get(id(&n.to_string())?)?;
            open()?.write_temp_patch(&p)?;
            println!("\"{}\" → Temp Patch, Rücklesen byteidentisch", p.name().trim_end());
        }
        _ => return Err("lib import|list|show|diff|tag|fav|note|rm|export|send — siehe jd990 ohne Argumente".into()),
    }
    Ok(())
}
