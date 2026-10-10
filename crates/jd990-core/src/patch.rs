//! Ein Patch: 486 Byte, Patch Common & Effect (118) + Tone A–D (je 92).
//!
//! Der Patch hält die **Rohbytes** und nichts sonst. Werte werden beim Lesen
//! nie geklemmt oder korrigiert — ein Patch mit Karten-Wellenformen oder
//! unbekannten Werten läuft byte-exakt durch (R7). Nur `set` prüft Bereiche.

use crate::params::{self, COMMON_SIZE, NAME_LENGTH, NAME_OFFSET, PATCH_SIZE, ParamDef, Tone};
use crate::sysex::{Address, DT1_MAX_DATA, Dt1, SysexError, split_messages};

#[derive(Clone, PartialEq, Eq)]
pub struct Patch {
    data: [u8; PATCH_SIZE],
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SetError {
    OutOfRange { id: &'static str, value: u16, min: u16, max: u16 },
}

impl std::fmt::Display for SetError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            SetError::OutOfRange { id, value, min, max } => write!(f, "{id}: {value} außerhalb {min}…{max}"),
        }
    }
}

impl std::error::Error for SetError {}

impl std::fmt::Debug for Patch {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Patch({:?})", self.name())
    }
}

impl Patch {
    pub fn from_bytes(data: [u8; PATCH_SIZE]) -> Patch {
        Patch { data }
    }

    /// Init-Patch aus der Spec (`[init]`): gültig, trocken, nur Tone A aktiv.
    pub fn init() -> Patch {
        Patch { data: params::INIT_PATCH }
    }

    /// Setzt einen Rohwert ohne Bereichsprüfung — nur für Importe, die fremde Werte
    /// unverändert übernehmen müssen (R7). Nicht für Eingaben.
    pub(crate) fn set_unchecked(&mut self, def: &ParamDef, tone: Option<Tone>, raw: u16) {
        let o = Self::abs_offset(def, tone);
        if def.bytes == 2 {
            self.data[o] = (raw >> 7) as u8 & 0x7F;
            self.data[o + 1] = raw as u8 & 0x7F;
        } else {
            self.data[o] = raw as u8 & 0x7F;
        }
    }

    pub(crate) fn set_name_byte(&mut self, i: usize, b: u8) {
        if i < NAME_LENGTH {
            self.data[NAME_OFFSET + i] = b & 0x7F;
        }
    }

    /// Alle Parameter, in denen sich `self` und `other` unterscheiden (Rohwerte).
    /// Der Name zählt nicht als Parameter — dafür `name()` vergleichen.
    pub fn diff_params(&self, other: &Patch) -> Vec<(&'static ParamDef, Option<Tone>, u16, u16)> {
        let mut out = Vec::new();
        for def in params::PATCH_PARAMS {
            let (a, b) = (self.get(def, None), other.get(def, None));
            if a != b {
                out.push((def, None, a, b));
            }
        }
        for t in Tone::ALL {
            for def in params::TONE_PARAMS {
                let (a, b) = (self.get(def, Some(t)), other.get(def, Some(t)));
                if a != b {
                    out.push((def, Some(t), a, b));
                }
            }
        }
        out
    }

    /// Kopiert Tone-Parameter von `from` nach `to` — alle (`group` = None) oder nur
    /// eine Gruppe der Spec (z. B. "TVF ENV"). Patch-weite Werte je Tone (Key Range,
    /// Velocity) bleiben, wie beim Tone-Kopieren am Gerät.
    pub fn copy_tone(&mut self, from: Tone, to: Tone, group: Option<&str>) {
        if from == to {
            return;
        }
        for def in params::TONE_PARAMS {
            if group.is_none_or(|g| def.group == g) {
                let (a, b) = (from.base() + def.offset, to.base() + def.offset);
                for i in 0..def.bytes {
                    self.data[b + i] = self.data[a + i];
                }
            }
        }
    }

    /// Tauscht zwei Tones komplett (alle 92 Byte).
    pub fn swap_tones(&mut self, a: Tone, b: Tone) {
        if a == b {
            return;
        }
        for i in 0..params::TONE_SIZE {
            self.data.swap(a.base() + i, b.base() + i);
        }
    }

    /// Stabiler Inhalts-Hash (FNV-1a 64) über die 486 Byte — für Dublettenerkennung.
    pub fn content_hash(&self) -> u64 {
        self.data.iter().fold(0xcbf29ce484222325u64, |h, &b| (h ^ b as u64).wrapping_mul(0x100000001b3))
    }

    /// DT1-Nachrichten, die `self` auf dem Gerät in `target` überführen — je
    /// geändertem Parameter eine Nachricht (2-Byte-Parameter immer ab MSB), der
    /// Name als ein Block. Für Undo, A/B-Vergleich, Librarian („nur Unterschiede senden“).
    pub fn diff_dt1(&self, target: &Patch, device: u8, base: Address) -> Vec<Dt1> {
        let mut out = Vec::new();
        let name = NAME_OFFSET..NAME_OFFSET + NAME_LENGTH;
        if self.data[name.clone()] != target.data[name.clone()] {
            out.push(Dt1 { device, address: base.offset(NAME_OFFSET), data: target.data[name].to_vec() });
        }
        let mut push = |def: &ParamDef, tone: Option<Tone>| {
            let o = Self::abs_offset(def, tone);
            if self.data[o..o + def.bytes] != target.data[o..o + def.bytes] {
                out.push(Dt1 { device, address: base.offset(o), data: target.data[o..o + def.bytes].to_vec() });
            }
        };
        for def in params::PATCH_PARAMS {
            push(def, None);
        }
        for t in Tone::ALL {
            for def in params::TONE_PARAMS {
                push(def, Some(t));
            }
        }
        out
    }

    pub fn as_bytes(&self) -> &[u8; PATCH_SIZE] {
        &self.data
    }

    /// Setzt einen Patch aus DT1-Nachrichten zusammen (beliebige Stückelung,
    /// beliebige Reihenfolge). Maßgeblich ist nur der Offset im Patch-Block;
    /// alle Nachrichten müssen im selben Block liegen und ihn lückenlos füllen.
    pub fn from_dt1(msgs: &[Dt1]) -> Result<(Patch, Address), SysexError> {
        let base = msgs.first().ok_or(SysexError::Incomplete)?.address.patch_base();
        let mut data = [0u8; PATCH_SIZE];
        let mut filled = [false; PATCH_SIZE];
        for m in msgs {
            if m.address.patch_base() != base {
                return Err(SysexError::Incomplete);
            }
            let start = m.address.patch_offset();
            if start + m.data.len() > PATCH_SIZE {
                return Err(SysexError::Incomplete);
            }
            data[start..start + m.data.len()].copy_from_slice(&m.data);
            filled[start..start + m.data.len()].iter_mut().for_each(|f| *f = true);
        }
        if filled.iter().all(|&f| f) { Ok((Patch { data }, base)) } else { Err(SysexError::Incomplete) }
    }

    /// Liest den (ersten) Patch aus einer .syx-Datei.
    pub fn from_syx(bytes: &[u8]) -> Result<(Patch, Address), SysexError> {
        let msgs = split_messages(bytes).into_iter().map(Dt1::decode).collect::<Result<Vec<_>, _>>()?;
        Self::from_dt1(&msgs)
    }

    /// Alle Patches aus einem Bytestrom (.syx oder .mid) — z. B. ein Bank-Dump.
    /// Berücksichtigt nur Patch-Bereiche (02 Performance-Temp-Patch, 03 Temp,
    /// 06 Internal, 0A Card); System-, Performance- und Rhythm-Daten werden
    /// übersprungen. Reihenfolge = aufsteigende Adresse.
    pub fn all_from_bytes(bytes: &[u8]) -> Result<Vec<(Patch, Address)>, Box<dyn std::error::Error>> {
        let owned;
        let bytes = if crate::smf::is_smf(bytes) {
            owned = crate::smf::extract_sysex(bytes)?;
            &owned[..]
        } else {
            bytes
        };
        let mut groups: std::collections::BTreeMap<Address, Vec<Dt1>> = Default::default();
        for m in split_messages(bytes) {
            // Nachrichten anderer Geräte/Kommandos (z. B. JD-800, Model 3DH) überspringen —
            // eine Prüfsumme, die bei einer JD-990-Nachricht nicht stimmt, ist dagegen ein Fehler.
            let d = match Dt1::decode(m) {
                Ok(d) => d,
                Err(SysexError::WrongModel(_) | SysexError::NotDt1(_) | SysexError::NotRoland | SysexError::TooShort) => continue,
                Err(e) => return Err(e.into()),
            };
            if matches!(d.address.area(), 0x02 | 0x03 | 0x06 | 0x0A) {
                groups.entry(d.address.patch_base()).or_default().push(d);
            }
        }
        Ok(groups.into_values().map(|g| Self::from_dt1(&g)).collect::<Result<Vec<_>, _>>()?)
    }

    /// Kodiert den Patch als DT1-Nachrichten **so, wie das Gerät selbst sendet**:
    /// erste Nachricht endet an Tone A (118 Byte), danach Stücke à max. 256 Byte
    /// (118 + 256 + 112). Das Gerät akzeptiert dieselbe Stückelung beim Empfang
    /// (64/64 byteidentisch, Logbuch 2026-09-28).
    pub fn to_dt1(&self, device: u8, base: Address) -> Vec<Dt1> {
        let mut out = Vec::new();
        let mut pos = 0;
        while pos < PATCH_SIZE {
            let mut end = (pos + DT1_MAX_DATA).min(PATCH_SIZE);
            if pos < COMMON_SIZE && end > COMMON_SIZE {
                end = COMMON_SIZE;
            }
            out.push(Dt1 { device, address: base.offset(pos), data: self.data[pos..end].to_vec() });
            pos = end;
        }
        out
    }

    pub fn to_syx(&self, device: u8, base: Address) -> Vec<u8> {
        self.to_dt1(device, base).iter().flat_map(Dt1::encode).collect()
    }

    pub fn name(&self) -> String {
        self.data[NAME_OFFSET..NAME_OFFSET + NAME_LENGTH]
            .iter()
            .map(|&b| if (0x20..0x7F).contains(&b) { b as char } else { '?' })
            .collect()
    }

    fn abs_offset(def: &ParamDef, tone: Option<Tone>) -> usize {
        match tone {
            Some(t) => t.base() + def.offset,
            None => def.offset,
        }
    }

    /// Rohwert eines Parameters. `tone` muss bei Tone-Parametern gesetzt sein.
    pub fn get(&self, def: &ParamDef, tone: Option<Tone>) -> u16 {
        let o = Self::abs_offset(def, tone);
        match def.bytes {
            2 => (self.data[o] as u16) << 7 | self.data[o + 1] as u16,
            _ => self.data[o] as u16,
        }
    }

    /// Setzt einen Rohwert — nur innerhalb des Spec-Bereichs. Gibt die geänderten
    /// Bytes als (Offset, Bytes) zurück, bereit für ein DT1 (bei 2-Byte-Parametern
    /// immer ab MSB, weil das LSB eine „#“-Adresse ist).
    pub fn set(&mut self, def: &'static ParamDef, tone: Option<Tone>, raw: u16) -> Result<(usize, Vec<u8>), SetError> {
        if !def.in_range(raw) {
            return Err(SetError::OutOfRange { id: def.id, value: raw, min: def.min, max: def.max });
        }
        let o = Self::abs_offset(def, tone);
        let bytes = match def.bytes {
            2 => vec![(raw >> 7) as u8, (raw & 0x7F) as u8],
            _ => vec![raw as u8],
        };
        self.data[o..o + bytes.len()].copy_from_slice(&bytes);
        Ok((o, bytes))
    }

    /// Alle Parameter mit Rohwert außerhalb des Spec-Bereichs (z. B. Karten-Wellenformen
    /// oder fremde/defekte Patches). Reine Diagnose — der Patch bleibt unverändert.
    pub fn out_of_range(&self) -> Vec<(&'static ParamDef, Option<Tone>, u16)> {
        let mut out = Vec::new();
        for def in params::PATCH_PARAMS {
            let v = self.get(def, None);
            if !def.in_range(v) {
                out.push((def, None, v));
            }
        }
        for t in Tone::ALL {
            for def in params::TONE_PARAMS {
                let v = self.get(def, Some(t));
                if !def.in_range(v) {
                    out.push((def, Some(t), v));
                }
            }
        }
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::params::find;

    fn corpus() -> Vec<(std::path::PathBuf, Vec<u8>)> {
        let dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../corpus");
        let mut out = Vec::new();
        let Ok(entries) = std::fs::read_dir(&dir) else { return out };
        for e in entries.flatten() {
            let Ok(files) = std::fs::read_dir(e.path()) else { continue };
            for f in files.flatten() {
                let name = f.file_name().to_string_lossy().to_string();
                if name.starts_with("patch-I") && name.ends_with(".syx") {
                    out.push((f.path(), std::fs::read(f.path()).unwrap()));
                }
            }
        }
        out.sort();
        out
    }

    /// ADR-6, Ebene 1: parse → serialize muss die Gerätebytes exakt reproduzieren —
    /// inkl. Stückelung, Adressen und Prüfsummen. Korpus ist lokal (gitignored);
    /// ohne Korpus wird der Test übersprungen.
    #[test]
    fn round_trip_gegen_das_geraete_backup() {
        let corpus = corpus();
        if corpus.is_empty() {
            eprintln!("kein Korpus unter corpus/ — Round-Trip übersprungen");
            return;
        }
        for (path, bytes) in &corpus {
            let (patch, base) = Patch::from_syx(bytes).unwrap_or_else(|e| panic!("{}: {e}", path.display()));
            assert_eq!(&patch.to_syx(0x10, base), bytes, "{}: nicht byteidentisch", path.display());
            assert!(patch.out_of_range().is_empty(), "{}: {:?}", path.display(), patch.out_of_range());
        }
        eprintln!("Round-Trip: {} Patches byteidentisch", corpus.len());
    }

    #[test]
    fn bank_dump_mit_64_patches() {
        let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../corpus/2026-09-27-backup/patches-internal.syx");
        let Ok(bytes) = std::fs::read(&path) else { return };
        let all = Patch::all_from_bytes(&bytes).unwrap();
        assert_eq!(all.len(), 64);
        assert_eq!(all[0].1, Address::internal_patch(0));
        assert_eq!(all[63].1, Address::internal_patch(63));
        let again: Vec<u8> = all.iter().flat_map(|(p, a)| p.to_syx(0x10, *a)).collect();
        assert_eq!(again, bytes, "Bank-Round-Trip nicht byteidentisch");
    }

    #[test]
    fn init_patch_ist_gueltig() {
        let p = Patch::init();
        assert_eq!(p.name(), "INIT PATCH      ");
        assert!(p.out_of_range().is_empty(), "{:?}", p.out_of_range());
        assert_eq!(p.get(find("tone.tvf.cutoff").unwrap(), Some(Tone::D)), 100);
        assert_eq!(p.get(find("tone.wg.pitch_coarse").unwrap(), Some(Tone::A)), 48); // 0 st
        assert_eq!(p.get(find("patch.pan").unwrap(), None), 50);
        assert_eq!(find("tone.tvf.cutoff").unwrap().init, 100);
    }

    #[test]
    fn diff_dt1_ueberfuehrt_patch_a_in_b() {
        let a = Patch::init();
        let mut b = Patch::init();
        b.set(find("tone.tvf.cutoff").unwrap(), Some(Tone::B), 33).unwrap();
        b.set(find("tone.wg.waveform").unwrap(), Some(Tone::A), 150).unwrap();
        let msgs = a.diff_dt1(&b, 0x10, Address::TEMP_PATCH);
        assert_eq!(msgs.len(), 2);
        assert_eq!(msgs.iter().find(|m| m.data.len() == 2).unwrap().address.patch_offset(), 0x77); // ab MSB
        // Nachrichten auf a angewendet ergeben b
        let mut bytes = *a.as_bytes();
        for m in &msgs {
            let o = m.address.patch_offset();
            bytes[o..o + m.data.len()].copy_from_slice(&m.data);
        }
        assert_eq!(&bytes, b.as_bytes());
        assert!(a.diff_dt1(&a, 0x10, Address::TEMP_PATCH).is_empty());
    }

    #[test]
    fn tones_kopieren_und_tauschen() {
        let mut p = Patch::init();
        let (cut, env, wave) = (find("tone.tvf.cutoff").unwrap(), find("tone.tvf_env.time_1").unwrap(), find("tone.wg.waveform").unwrap());
        p.set(cut, Some(Tone::A), 11).unwrap();
        p.set(env, Some(Tone::A), 22).unwrap();
        p.set(wave, Some(Tone::A), 150).unwrap();
        // nur Gruppe TVF ENV
        let mut q = p.clone();
        q.copy_tone(Tone::A, Tone::C, Some("TVF ENV"));
        assert_eq!(q.get(env, Some(Tone::C)), 22);
        assert_eq!(q.get(cut, Some(Tone::C)), 100);
        // ganzer Tone (inkl. 2-Byte-Wellenform)
        q.copy_tone(Tone::A, Tone::D, None);
        assert_eq!((q.get(cut, Some(Tone::D)), q.get(wave, Some(Tone::D))), (11, 150));
        // tauschen ist umkehrbar
        let mut r = p.clone();
        r.swap_tones(Tone::A, Tone::B);
        assert_eq!((r.get(cut, Some(Tone::B)), r.get(cut, Some(Tone::A))), (11, 100));
        r.swap_tones(Tone::A, Tone::B);
        assert_eq!(r, p);
    }

    #[test]
    fn stueckelung_wie_das_geraet() {
        let p = Patch::from_bytes([0; PATCH_SIZE]);
        let sizes: Vec<_> = p.to_dt1(0x10, Address::TEMP_PATCH).iter().map(|m| m.data.len()).collect();
        assert_eq!(sizes, [118, 256, 112]);
        let addrs: Vec<_> = p.to_dt1(0x10, Address::TEMP_PATCH).iter().map(|m| m.address.to_bytes()).collect();
        assert_eq!(addrs, [[3, 0, 0, 0], [3, 0, 0, 0x76], [3, 0, 2, 0x76]]);
    }

    #[test]
    fn set_prueft_bereich_und_liefert_dt1_bytes() {
        let mut p = Patch::from_bytes([0; PATCH_SIZE]);
        let cutoff = find("tone.tvf.cutoff").unwrap();
        assert_eq!(p.set(cutoff, Some(Tone::C), 100), Ok((0x12E + 25, vec![100])));
        assert_eq!(p.get(cutoff, Some(Tone::C)), 100);
        assert!(p.set(cutoff, Some(Tone::C), 101).is_err());
        let wave = find("tone.wg.waveform").unwrap();
        assert_eq!(p.set(wave, Some(Tone::A), 194), Ok((0x77, vec![1, 66])));
        assert_eq!(p.get(wave, Some(Tone::A)), 194);
    }

    #[test]
    fn unbekannte_werte_laufen_unveraendert_durch() {
        // R7: z. B. Karten-Wellenform 250 bei Source CARD — nie klemmen.
        let mut raw = [0u8; PATCH_SIZE];
        raw[0x76] = 1; // Source CARD
        raw[0x77] = 1;
        raw[0x78] = 122; // Waveform 250
        raw[0x76 + 25] = 0x7F; // Cutoff 127 — außerhalb 0…100
        let p = Patch::from_bytes(raw);
        let (back, _) = Patch::from_syx(&p.to_syx(0x10, Address::TEMP_PATCH)).unwrap();
        assert_eq!(back.as_bytes(), &raw);
        // Gemeldet: Cutoff 127 und die vier Velocity Points (0 < min 1) — die
        // Karten-Wellenform 250 liegt im Spec-Bereich 0…255 und ist kein Befund.
        let ids: Vec<_> = back.out_of_range().iter().map(|(d, t, v)| (d.id, *t, *v)).collect();
        assert!(ids.contains(&("tone.tvf.cutoff", Some(Tone::A), 127)), "{ids:?}");
        assert_eq!(ids.len(), 5, "{ids:?}");
    }
}
