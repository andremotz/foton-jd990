//! Performance (ADR-22): 144 Byte = Common 48 (Name, Sync-Part, Stimmenreservierung,
//! Chorus/Delay/Reverb) + Part 1–8 je 12 Byte — und das **Setup**: Performance plus
//! die Temp-Patches der Parts 1–7, die Einheit, die der Editor speichert und sendet.
//!
//! Wie beim Patch werden Rohbytes nie geklemmt; nur `set` prüft und wendet die
//! Geräteregel für die Stimmenreservierung an (Summe ≤ 24, Block immer ganz).

use crate::display;
use crate::params::{self, PART_BASE, PART_COUNT, PART_PARAMS, PART_SIZE, PERF_NAME_LENGTH, PERF_NAME_OFFSET, PERF_PARAMS, PERF_SIZE, ParamDef};
use crate::patch::{Patch, SetError};
use crate::sysex::{Address, Dt1, SysexError, split_messages};

/// Stimmen des JD-990.
pub const VOICES: u16 = 24;
/// Parts mit eigenem Patch (Part 8 ist der Rhythm-Part, out of scope).
pub const PATCH_PARTS: usize = 7;
/// Stimmenreservierung Part 1–8: ein Block, den das Gerät nur als Ganzes annimmt
/// („#“ ab Part 2 — ein DT1 darf dort nicht beginnen).
const RESERVE_OFFSET: usize = 17;

#[derive(Clone, PartialEq, Eq)]
pub struct Performance {
    data: [u8; PERF_SIZE],
}

impl std::fmt::Debug for Performance {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Performance({:?})", self.name())
    }
}

impl Performance {
    pub fn from_bytes(data: [u8; PERF_SIZE]) -> Performance {
        Performance { data }
    }

    /// Startpunkt für „neue Performance“ (eigene Festlegung, kein Roland-Wert):
    /// Parts 1–7 empfangen auf Kanal 1–7 und spielen I:11 … I:17, Part 8 aus;
    /// Stimmen 4/4/4/3/3/3/3/0; Effekte trocken, Send DRY.
    pub fn init() -> Performance {
        let mut p = Performance { data: [0; PERF_SIZE] };
        for (i, b) in format!("{:<16}", "INIT PERFORM").bytes().enumerate() {
            p.data[PERF_NAME_OFFSET + i] = b;
        }
        for def in PERF_PARAMS {
            p.put(def, None, def.init);
        }
        let reserve = [4u16, 4, 4, 3, 3, 3, 3, 0];
        for (i, v) in reserve.iter().enumerate() {
            p.data[RESERVE_OFFSET + i] = *v as u8;
        }
        for part in 0..PART_COUNT {
            for def in PART_PARAMS {
                p.put(def, Some(part), def.init);
            }
            let on = part < PATCH_PARTS;
            p.put(params::find_perf("part.rx_sw").unwrap(), Some(part), on as u16);
            p.put(params::find_perf("part.rx_ch").unwrap(), Some(part), if on { part as u16 } else { 9 });
            p.put(params::find_perf("part.patch").unwrap(), Some(part), if on { part as u16 } else { 0 });
            p.put(params::find_perf("part.level").unwrap(), Some(part), 100);
        }
        p
    }

    pub fn as_bytes(&self) -> &[u8; PERF_SIZE] {
        &self.data
    }

    pub fn name(&self) -> String {
        self.data[PERF_NAME_OFFSET..PERF_NAME_OFFSET + PERF_NAME_LENGTH]
            .iter()
            .map(|&b| if (0x20..0x7F).contains(&b) { b as char } else { '?' })
            .collect()
    }

    /// Neuer Name (ASCII, auf 16 Zeichen gekürzt/aufgefüllt) — liefert das DT1-Datenstück.
    pub fn set_name(&mut self, name: &str) -> (usize, Vec<u8>) {
        let bytes: Vec<u8> = format!("{name:<16}").bytes().take(PERF_NAME_LENGTH).map(|b| if (0x20..0x7F).contains(&b) { b } else { b'?' }).collect();
        self.data[PERF_NAME_OFFSET..PERF_NAME_OFFSET + PERF_NAME_LENGTH].copy_from_slice(&bytes);
        (PERF_NAME_OFFSET, bytes)
    }

    fn abs_offset(def: &ParamDef, part: Option<usize>) -> usize {
        part.map_or(0, |p| PART_BASE + p * PART_SIZE) + def.offset
    }

    fn put(&mut self, def: &ParamDef, part: Option<usize>, raw: u16) {
        let o = Self::abs_offset(def, part);
        if def.bytes == 2 {
            self.data[o] = (raw >> 7) as u8 & 0x7F;
            self.data[o + 1] = raw as u8 & 0x7F;
        } else {
            self.data[o] = raw as u8 & 0x7F;
        }
    }

    /// Rohwert; `part` (0–7) bei Part-Parametern, `None` bei Common-Parametern.
    pub fn get(&self, def: &ParamDef, part: Option<usize>) -> u16 {
        let o = Self::abs_offset(def, part);
        match def.bytes {
            2 => (self.data[o] as u16) << 7 | self.data[o + 1] as u16,
            _ => self.data[o] as u16,
        }
    }

    pub fn display(&self, def: &ParamDef, part: Option<usize>) -> String {
        display::format(def, self.get(def, part))
    }

    fn is_reserve(def: &ParamDef) -> bool {
        def.id.starts_with("perf.reserve_")
    }

    /// Summe der reservierten Stimmen (Part 1–8).
    pub fn voices_reserved(&self) -> u16 {
        self.data[RESERVE_OFFSET..RESERVE_OFFSET + 8].iter().map(|&b| b as u16).sum()
    }

    /// Setzt einen Rohwert (Bereich geprüft) und liefert (Offset, Bytes) fürs DT1.
    /// Stimmenreservierung: höchstens so viel, wie die anderen Parts übrig lassen
    /// (Summe ≤ 24); gesendet wird immer der ganze Block Part 1–8.
    pub fn set(&mut self, def: &'static ParamDef, part: Option<usize>, raw: u16) -> Result<(usize, Vec<u8>), SetError> {
        if !def.in_range(raw) {
            return Err(SetError::OutOfRange { id: def.id, value: raw, min: def.min, max: def.max });
        }
        if Self::is_reserve(def) {
            let mine = self.get(def, None);
            let others = self.voices_reserved() - mine;
            self.put(def, None, raw.min(VOICES.saturating_sub(others)));
            return Ok((RESERVE_OFFSET, self.data[RESERVE_OFFSET..RESERVE_OFFSET + 8].to_vec()));
        }
        self.put(def, part, raw);
        let o = Self::abs_offset(def, part);
        Ok((o, self.data[o..o + def.bytes].to_vec()))
    }

    /// Lesbarer Patch-Platz eines Parts: „I:11“, „C:25“, „A:11“, „B:88“.
    pub fn patch_slot(&self, part: usize) -> String {
        let bank = self.get(params::find_perf("part.bank").unwrap(), Some(part));
        let n = self.get(params::find_perf("part.patch").unwrap(), Some(part));
        let letter = match (bank, n < 64) {
            (0, true) => 'I',
            (0, false) => 'C',
            (_, true) => 'A',
            (_, false) => 'B',
        };
        let k = n % 64;
        format!("{letter}:{}{}", k / 8 + 1, k % 8 + 1)
    }

    /// Interne Patchnummer (0–63) des Parts, falls er auf einen internen Patch zeigt.
    pub fn internal_patch_of(&self, part: usize) -> Option<u8> {
        let bank = self.get(params::find_perf("part.bank").unwrap(), Some(part));
        let n = self.get(params::find_perf("part.patch").unwrap(), Some(part));
        (bank == 0 && n < 64).then_some(n as u8)
    }

    pub fn to_dt1(&self, device: u8, base: Address) -> Vec<Dt1> {
        vec![Dt1 { device, address: base, data: self.data.to_vec() }]
    }

    pub fn to_syx(&self, device: u8, base: Address) -> Vec<u8> {
        self.to_dt1(device, base).iter().flat_map(Dt1::encode).collect()
    }

    /// DT1-Nachrichten, die `self` auf dem Gerät in `target` überführen (nur Unterschiede).
    pub fn diff_dt1(&self, target: &Performance, device: u8, base: Address) -> Vec<Dt1> {
        let mut out = Vec::new();
        let mut range = |start: usize, len: usize| {
            if self.data[start..start + len] != target.data[start..start + len] {
                out.push(Dt1 { device, address: base.offset(start), data: target.data[start..start + len].to_vec() });
            }
        };
        range(PERF_NAME_OFFSET, PERF_NAME_LENGTH);
        range(RESERVE_OFFSET, 8);
        for def in PERF_PARAMS.iter().filter(|d| !Self::is_reserve(d)) {
            range(def.offset, def.bytes);
        }
        for part in 0..PART_COUNT {
            for def in PART_PARAMS {
                range(Self::abs_offset(def, Some(part)), def.bytes);
            }
        }
        out
    }

    /// Setzt eine Performance aus DT1-Nachrichten zusammen. Daten hinter Byte 144
    /// werden ignoriert (frühe Backups haben 572 Byte angefragt — Logbuch 2026-09-28).
    pub fn from_dt1(msgs: &[Dt1]) -> Result<(Performance, Address), SysexError> {
        let base = msgs.first().ok_or(SysexError::Incomplete)?.address.patch_base();
        let mut data = [0u8; PERF_SIZE];
        let mut filled = [false; PERF_SIZE];
        for m in msgs {
            if m.address.patch_base() != base {
                return Err(SysexError::Incomplete);
            }
            let start = m.address.patch_offset();
            for (i, &b) in m.data.iter().enumerate() {
                if let Some(slot) = data.get_mut(start + i) {
                    *slot = b;
                    filled[start + i] = true;
                }
            }
        }
        if filled.iter().all(|&f| f) { Ok((Performance { data }, base)) } else { Err(SysexError::Incomplete) }
    }

    /// Alle Performances (Temp `01`, Internal `05`, Card `09`) aus einem Bytestrom.
    pub fn all_from_bytes(bytes: &[u8]) -> Result<Vec<(Performance, Address)>, Box<dyn std::error::Error>> {
        let owned;
        let bytes = if crate::smf::is_smf(bytes) {
            owned = crate::smf::extract_sysex(bytes)?;
            &owned[..]
        } else {
            bytes
        };
        let mut groups: std::collections::BTreeMap<Address, Vec<Dt1>> = Default::default();
        for m in split_messages(bytes) {
            let d = match Dt1::decode(m) {
                Ok(d) => d,
                Err(SysexError::WrongModel(_) | SysexError::NotDt1(_) | SysexError::NotRoland | SysexError::TooShort) => continue,
                // Prüfsummenfehler hinter Byte 144 (Über-Anfrage) nicht fatal: Nachricht überspringen
                Err(SysexError::Checksum { address, .. }) if address.patch_offset() >= PERF_SIZE => continue,
                Err(e) => return Err(e.into()),
            };
            if matches!(d.address.area(), 0x01 | 0x05 | 0x09) && d.address.patch_offset() < PERF_SIZE {
                groups.entry(d.address.patch_base()).or_default().push(d);
            }
        }
        Ok(groups.into_values().map(|g| Self::from_dt1(&g)).collect::<Result<Vec<_>, _>>()?)
    }
}

/// Performance + die Patches der Parts 1–7: das, was im Performance-Modus klingt.
#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Setup {
    pub performance: Performance,
    pub patches: [Patch; PATCH_PARTS],
}

impl Setup {
    pub fn init() -> Setup {
        Setup { performance: Performance::init(), patches: std::array::from_fn(|_| Patch::init()) }
    }

    /// Alles für die Temp-Bereiche: erst die Performance (`01`), dann die Part-Patches
    /// (`02 0n`). Reihenfolge wichtig: Ein Patchwechsel am Part lädt den Speicherpatch
    /// nach `02 0n` — die Patches müssen danach kommen (zu prüfen in P0).
    pub fn to_syx(&self, device: u8) -> Vec<u8> {
        let mut out = self.performance.to_syx(device, Address::TEMP_PERFORMANCE);
        for (i, p) in self.patches.iter().enumerate() {
            out.extend(p.to_syx(device, Address::perf_patch(i as u8)));
        }
        out
    }

    /// Setup aus einem Dump der Temp-Bereiche (`01` + `02 00…06`), z. B. einer
    /// gespeicherten Setup-Datei. Fehlt die Performance oder ein Part-Patch: Fehler.
    pub fn from_bytes(bytes: &[u8]) -> Result<Setup, Box<dyn std::error::Error>> {
        let perf = Performance::all_from_bytes(bytes)?
            .into_iter()
            .find(|(_, a)| *a == Address::TEMP_PERFORMANCE)
            .ok_or("keine Temp-Performance (01 00 00 00) in den Daten")?
            .0;
        let patches = Patch::all_from_bytes(bytes)?;
        let mut parts: [Option<Patch>; PATCH_PARTS] = Default::default();
        for (p, a) in patches {
            if a.area() == 0x02 {
                let n = ((a.0 >> 14) & 0x7F) as usize;
                if n < PATCH_PARTS {
                    parts[n] = Some(p);
                }
            }
        }
        let missing: Vec<usize> = (0..PATCH_PARTS).filter(|&i| parts[i].is_none()).map(|i| i + 1).collect();
        if !missing.is_empty() {
            return Err(format!("Part-Patches fehlen: {missing:?}").into());
        }
        Ok(Setup { performance: perf, patches: parts.map(|p| p.unwrap()) })
    }

    /// Setup aus einer internen Performance und den 64 internen Patches (Backup).
    /// Parts, die auf Card/Preset zeigen, bekommen den Init-Patch — ihre Nummern
    /// werden zurückgegeben (die Daten liegen nicht im Backup, nur im Gerät).
    pub fn from_internal(performance: Performance, internal: &[Patch]) -> (Setup, Vec<usize>) {
        let mut unresolved = Vec::new();
        let patches = std::array::from_fn(|i| match performance.internal_patch_of(i).and_then(|n| internal.get(n as usize)) {
            Some(p) => p.clone(),
            None => {
                unresolved.push(i + 1);
                Patch::init()
            }
        });
        (Setup { performance, patches }, unresolved)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::params::find_perf;

    fn backups() -> Vec<Vec<u8>> {
        let dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../corpus");
        let mut out = Vec::new();
        let Ok(entries) = std::fs::read_dir(&dir) else { return out };
        for e in entries.flatten() {
            let Ok(files) = std::fs::read_dir(e.path()) else { continue };
            for f in files.flatten() {
                let name = f.file_name().to_string_lossy().to_string();
                if name.starts_with("performance-") && name.ends_with(".syx") {
                    out.push(std::fs::read(f.path()).unwrap());
                }
            }
        }
        out
    }

    #[test]
    fn init_ist_gueltig() {
        let p = Performance::init();
        assert_eq!(p.name().trim_end(), "INIT PERFORM");
        assert_eq!(p.voices_reserved(), 24);
        for def in PERF_PARAMS {
            assert!(def.in_range(p.get(def, None)), "{}", def.id);
        }
        for part in 0..8 {
            for def in PART_PARAMS {
                assert!(def.in_range(p.get(def, Some(part))), "{} Part {}", def.id, part + 1);
            }
        }
        assert_eq!(p.get(find_perf("part.rx_ch").unwrap(), Some(2)), 2); // Kanal 3
        assert_eq!(p.patch_slot(2), "I:13");
    }

    #[test]
    fn stimmen_summe_hoechstens_24_block_ganz() {
        let mut p = Performance::init(); // 4/4/4/3/3/3/3/0 = 24
        let r1 = find_perf("perf.reserve_1").unwrap();
        let (off, bytes) = p.set(r1, None, 10).unwrap();
        assert_eq!((off, bytes.len()), (17, 8), "immer der ganze Block");
        assert_eq!(p.get(r1, None), 4, "nichts frei → bleibt");
        p.set(find_perf("perf.reserve_2").unwrap(), None, 0).unwrap();
        p.set(r1, None, 10).unwrap();
        assert_eq!(p.get(r1, None), 8, "nur die 4 frei gewordenen");
        assert_eq!(p.voices_reserved(), 24);
    }

    #[test]
    fn roundtrip_und_diff() {
        let a = Performance::init();
        let syx = a.to_syx(0x10, Address::TEMP_PERFORMANCE);
        let back = Performance::all_from_bytes(&syx).unwrap();
        assert_eq!(back, vec![(a.clone(), Address::TEMP_PERFORMANCE)]);
        let mut b = a.clone();
        b.set(find_perf("part.level").unwrap(), Some(4), 50).unwrap();
        let d = a.diff_dt1(&b, 0x10, Address::TEMP_PERFORMANCE);
        assert_eq!(d.len(), 1);
        assert_eq!(d[0].address.to_bytes(), [0x01, 0x00, 0x00, 0x64], "Part 5 Level = 0x30 + 4·12 + 4");
        assert_eq!(d[0].data, vec![50]);
    }

    #[test]
    fn setup_roundtrip_reihenfolge() {
        let mut s = Setup::init();
        s.patches[3].set(crate::params::find("tone.tvf.cutoff").unwrap(), Some(crate::Tone::A), 42).unwrap();
        let syx = s.to_syx(0x10);
        let msgs = split_messages(&syx);
        assert_eq!(&msgs[0][5..9], &[0x01, 0, 0, 0], "Performance zuerst");
        assert_eq!(&msgs.last().unwrap()[5..7], &[0x02, 0x06], "zuletzt Part 7");
        assert_eq!(Setup::from_bytes(&syx).unwrap(), s);
    }

    #[test]
    fn backup_performances_lesbar() {
        let files = backups();
        if files.is_empty() {
            return; // ohne Korpus (CI) nichts zu prüfen
        }
        let mut n = 0;
        for bytes in &files {
            for (p, a) in Performance::all_from_bytes(bytes).unwrap() {
                assert_eq!(a.area(), 0x05);
                assert!(p.voices_reserved() <= VOICES, "{:?}: {} Stimmen", p.name(), p.voices_reserved());
                for part in 0..8 {
                    assert_eq!(p.as_bytes()[PART_BASE + part * PART_SIZE + 11], 0, "Dummy-Byte Part {}", part + 1);
                    for def in PART_PARAMS {
                        assert!(def.in_range(p.get(def, Some(part))), "{:?} {} Part {}", p.name(), def.id, part + 1);
                    }
                }
                n += 1;
            }
        }
        assert!(n >= 16, "{n} Performances");
    }
}
