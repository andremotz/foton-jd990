//! Parser und Validierung für `spec/params.toml` — die Single Source of Truth (ADR-3).
//!
//! Wird zur Build-Zeit von `jd990-core` benutzt (Codegen) und in Tests, um die
//! Invarianten der Spec zu prüfen. Kennt weder MIDI noch UI.

use serde::Deserialize;
use std::collections::BTreeMap;
use std::path::Path;

#[derive(Debug, Deserialize)]
pub struct Spec {
    pub meta: Meta,
    pub enums: BTreeMap<String, EnumDef>,
    #[serde(default)]
    pub tables: BTreeMap<String, TableDef>,
    pub tone: ToneBlock,
    pub patch: PatchBlock,
    pub performance: PerformanceBlock,
    #[serde(default)]
    pub init: Init,
}

/// Performance (ADR-22): Common + `part_count` Parts gleicher Struktur.
#[derive(Debug, Deserialize)]
pub struct PerformanceBlock {
    pub size: usize,
    pub common_size: usize,
    pub name_offset: usize,
    pub name_length: usize,
    /// Offsets ohne Parameter (Dummy-Bytes) im Common bzw. im Part
    #[serde(default)]
    pub reserved: Vec<usize>,
    pub part_base: usize,
    pub part_size: usize,
    pub part_count: usize,
    #[serde(default)]
    pub part_reserved: Vec<usize>,
    pub group_order: Vec<String>,
    pub param: Vec<Param>,
    pub part_param: Vec<Param>,
}

/// Init-Patch: Regel + Ausnahmen (Rohwerte), siehe `[init]` in der Spec.
#[derive(Debug, Default, Deserialize)]
pub struct Init {
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub patch: BTreeMap<String, u16>,
    #[serde(default)]
    pub tone: BTreeMap<String, u16>,
}

#[derive(Debug, Deserialize)]
pub struct Meta {
    pub model_id: u8,
    pub spec_version: u32,
    pub tone_bases: ToneBases,
}

#[derive(Debug, Deserialize)]
pub struct ToneBases {
    pub size: usize,
    pub verified: bool,
    pub a: usize,
    pub b: usize,
    pub c: usize,
    pub d: usize,
}

impl ToneBases {
    pub fn all(&self) -> [usize; 4] {
        [self.a, self.b, self.c, self.d]
    }
}

#[derive(Debug, Deserialize)]
pub struct EnumDef {
    pub values: Vec<String>,
    pub verified: bool,
    #[serde(default)]
    pub out_of_scope: Vec<u16>,
}

#[derive(Debug, Deserialize)]
pub struct Segment {
    pub from: u16,
    pub start: f64,
    pub step: f64,
}

#[derive(Debug, Deserialize)]
pub struct NoteSegment {
    pub from: u16,
    pub values: Vec<String>,
}

#[derive(Debug, Deserialize)]
pub struct TableDef {
    pub verified: bool,
    #[serde(default)]
    pub unit: Option<String>,
    #[serde(default)]
    pub values: Option<Vec<String>>,
    #[serde(default)]
    pub segments: Option<Vec<Segment>>,
    #[serde(default)]
    pub notes: Option<NoteSegment>,
}

#[derive(Debug, Deserialize)]
pub struct ToneBlock {
    pub size: usize,
    pub group_order: Vec<String>,
    pub param: Vec<Param>,
}

#[derive(Debug, Deserialize)]
pub struct PatchBlock {
    pub size: usize,
    pub common_size: usize,
    pub name_offset: usize,
    pub name_length: usize,
    pub group_order: Vec<String>,
    pub param: Vec<Param>,
}

#[derive(Debug, Deserialize)]
pub struct RawRange {
    pub min: u16,
    pub max: u16,
}

#[derive(Debug, Deserialize)]
pub struct Display {
    pub kind: String,
    #[serde(default)]
    pub table: Option<String>,
    #[serde(default)]
    pub min: Option<f64>,
    #[serde(default)]
    pub max: Option<f64>,
    #[serde(default)]
    pub unit: Option<String>,
    #[serde(default)]
    pub extra: Vec<String>,
    /// Sonderbeschriftung einzelner Rohwerte, z. B. {0 = "OFF"}
    #[serde(default)]
    pub labels: BTreeMap<String, String>,
    #[serde(default)]
    pub center: Option<f64>,
    /// Anzeige-Nummer des Rohwerts 0 (Wellenformen: 1)
    #[serde(default)]
    pub first: Option<u16>,
}

#[derive(Debug, Deserialize)]
pub struct Param {
    pub id: String,
    pub offset: usize,
    #[serde(default = "one")]
    pub bytes: usize,
    pub group: String,
    pub label: String,
    pub raw: RawRange,
    pub display: Display,
    pub verified: bool,
}

fn one() -> usize {
    1
}

impl Param {
    /// Rohwert laut Init-Regel: bipolar linear → Anzeige 0, Pan → Mitte, sonst raw.min.
    pub fn init_rule(&self) -> u16 {
        let d = &self.display;
        match d.kind.as_str() {
            "linear" if d.min.unwrap_or(0.0) < 0.0 => {
                let (dmin, dmax) = (d.min.unwrap(), d.max.unwrap());
                let lin_max = self.raw.max - d.extra.len() as u16;
                let pos = (0.0 - dmin) / (dmax - dmin) * (lin_max - self.raw.min) as f64;
                self.raw.min + pos.round() as u16
            }
            "pan" => d.center.unwrap_or(50.0) as u16,
            _ => self.raw.min,
        }
    }
}

impl Spec {
    /// Init-Wert eines Parameters: Ausnahme aus `[init]`, sonst die Regel.
    pub fn init_value(&self, p: &Param) -> u16 {
        let table = if p.id.starts_with("tone.") { &self.init.tone } else { &self.init.patch };
        table.get(&p.id).copied().unwrap_or_else(|| p.init_rule())
    }

    pub fn load(path: impl AsRef<Path>) -> Result<Spec, String> {
        let text = std::fs::read_to_string(path.as_ref())
            .map_err(|e| format!("{}: {e}", path.as_ref().display()))?;
        toml::from_str(&text).map_err(|e| e.to_string())
    }

    /// Prüft die Invarianten der Spec. Leere Liste = alles in Ordnung.
    pub fn validate(&self) -> Vec<String> {
        let mut errors = Vec::new();
        coverage("Tone", &self.tone.param, self.tone.size, 0..0, &mut errors);
        let name = self.patch.name_offset..self.patch.name_offset + self.patch.name_length;
        coverage("Patch Common", &self.patch.param, self.patch.common_size, name, &mut errors);

        let bases = self.meta.tone_bases.all();
        for (i, b) in bases.iter().enumerate() {
            let want = self.patch.common_size + i * self.meta.tone_bases.size;
            if *b != want {
                errors.push(format!("Tone-Basis {i}: {b:#x}, erwartet {want:#x}"));
            }
        }
        if self.patch.common_size + 4 * self.tone.size != self.patch.size {
            errors.push("Patchgröße ≠ Common + 4 × Tone".into());
        }

        // Performance: Common (Name + Dummy reserviert) und ein Part — vollständig, ohne Überlappung
        let pf = &self.performance;
        let mut reserved: Vec<usize> = (pf.name_offset..pf.name_offset + pf.name_length).collect();
        reserved.extend(&pf.reserved);
        coverage_set("Performance Common", &pf.param, pf.common_size, &reserved, &mut errors);
        coverage_set("Performance Part", &pf.part_param, pf.part_size, &pf.part_reserved, &mut errors);
        if pf.part_base != pf.common_size || pf.part_base + pf.part_count * pf.part_size != pf.size {
            errors.push("Performance: Größe ≠ Common + Parts".into());
        }
        for p in pf.param.iter().chain(&pf.part_param) {
            if !pf.group_order.contains(&p.group) {
                errors.push(format!("{}: Gruppe {} fehlt in performance.group_order", p.id, p.group));
            }
        }

        for (table, is_tone) in [(&self.init.patch, false), (&self.init.tone, true)] {
            for (id, v) in table {
                let list = if is_tone { &self.tone.param } else { &self.patch.param };
                match list.iter().find(|p| &p.id == id) {
                    Some(p) if (p.raw.min..=p.raw.max).contains(v) => {}
                    Some(p) => errors.push(format!("init: {id} = {v} außerhalb {}…{}", p.raw.min, p.raw.max)),
                    None => errors.push(format!("init: {id} ist kein {}-Parameter", if is_tone { "Tone" } else { "Patch" })),
                }
            }
        }
        if self.init.name.len() > self.patch.name_length || !self.init.name.bytes().all(|b| (0x20..0x7F).contains(&b)) {
            errors.push("init.name: max. 16 Zeichen ASCII".into());
        }

        let mut ids = std::collections::BTreeSet::new();
        for p in self.tone.param.iter().chain(&self.patch.param).chain(&self.performance.param).chain(&self.performance.part_param) {
            if !ids.insert(&p.id) {
                errors.push(format!("{}: ID doppelt", p.id));
            }
            if p.raw.min > p.raw.max {
                errors.push(format!("{}: raw.min > raw.max", p.id));
            }
            let limit = if p.bytes == 2 { 0x3FFF } else { 0x7F };
            if p.raw.max > limit {
                errors.push(format!("{}: raw.max {} passt nicht in {} Byte", p.id, p.raw.max, p.bytes));
            }
            let n = (p.raw.max + 1) as usize;
            match p.display.kind.as_str() {
                "enum" => match p.display.table.as_ref().and_then(|t| self.enums.get(t)) {
                    Some(e) if e.values.len() == n => {}
                    Some(e) => errors.push(format!("{}: Enum hat {} Werte, raw erlaubt {n}", p.id, e.values.len())),
                    None => errors.push(format!("{}: Enum-Tabelle fehlt", p.id)),
                },
                "table" => match p.display.table.as_ref().and_then(|t| self.tables.get(t)) {
                    Some(t) => {
                        if let Some(v) = &t.values
                            && v.len() != n
                        {
                            errors.push(format!("{}: Tabelle hat {} Werte, raw erlaubt {n}", p.id, v.len()));
                        }
                        if let Some(s) = &t.segments {
                            let end = t.notes.as_ref().map_or(n, |x| x.from as usize + x.values.len());
                            if s.first().map(|x| x.from) != Some(0) || s.windows(2).any(|w| w[0].from >= w[1].from) || end != n {
                                errors.push(format!("{}: Segmente passen nicht zu raw 0…{}", p.id, p.raw.max));
                            }
                        }
                    }
                    None => errors.push(format!("{}: Tabelle fehlt", p.id)),
                },
                "linear" | "pan" | "note" | "wavelist" => {}
                k => errors.push(format!("{}: unbekannte Anzeige '{k}'", p.id)),
            }
        }
        errors
    }
}

fn coverage(what: &str, params: &[Param], size: usize, reserved: std::ops::Range<usize>, errors: &mut Vec<String>) {
    coverage_set(what, params, size, &reserved.collect::<Vec<_>>(), errors);
}

fn coverage_set(what: &str, params: &[Param], size: usize, reserved: &[usize], errors: &mut Vec<String>) {
    let mut seen = vec![0u8; size];
    for &o in reserved {
        if let Some(s) = seen.get_mut(o) {
            *s += 1;
        }
    }
    for p in params {
        for o in p.offset..p.offset + p.bytes {
            match seen.get_mut(o) {
                Some(s) => *s += 1,
                None => errors.push(format!("{}: Offset {o} außerhalb von {what}", p.id)),
            }
        }
    }
    let missing: Vec<_> = (0..size).filter(|&o| seen[o] == 0).collect();
    let double: Vec<_> = (0..size).filter(|&o| seen[o] > 1).collect();
    if !missing.is_empty() {
        errors.push(format!("{what}: Offsets ohne Parameter {missing:?}"));
    }
    if !double.is_empty() {
        errors.push(format!("{what}: Offsets doppelt belegt {double:?}"));
    }
}

#[derive(Debug, Deserialize)]
struct WaveformFile {
    waveforms: Waveforms,
}

#[derive(Debug, Deserialize)]
pub struct Waveforms {
    /// Anzahl echter Wellenformen (ohne „None“) — `names` hat count + 1 Einträge
    pub count: usize,
    /// Index 0 = "None"; Name einer Wellenform = names[rohwert + 1]
    pub names: Vec<String>,
}

pub fn load_waveforms(path: impl AsRef<Path>) -> Result<Waveforms, String> {
    let text = std::fs::read_to_string(path.as_ref()).map_err(|e| format!("{}: {e}", path.as_ref().display()))?;
    let f: WaveformFile = toml::from_str(&text).map_err(|e| e.to_string())?;
    if f.waveforms.names.len() != f.waveforms.count + 1 {
        return Err(format!("waveforms.toml: count {} + „None“ ≠ {} Namen", f.waveforms.count, f.waveforms.names.len()));
    }
    Ok(f.waveforms)
}

/// Pfad zur Spec relativ zu diesem Crate.
pub fn default_spec_path() -> std::path::PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../spec/params.toml")
}

pub fn default_waveforms_path() -> std::path::PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../spec/waveforms.toml")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn wellenformen_lesbar() {
        let w = load_waveforms(default_waveforms_path()).unwrap();
        assert_eq!(w.names.len(), w.count + 1);
        assert_eq!(w.names[0], "None");
    }

    #[test]
    fn spec_ist_gueltig() {
        let spec = Spec::load(default_spec_path()).expect("Spec lesbar");
        let errors = spec.validate();
        assert!(errors.is_empty(), "Spec-Fehler:\n{}", errors.join("\n"));
    }

    #[test]
    fn spec_deckt_den_ganzen_patch_ab() {
        let spec = Spec::load(default_spec_path()).unwrap();
        assert_eq!(spec.tone.param.len(), 91); // 92 Bytes, Waveform belegt 2
        assert_eq!(spec.patch.size, 486);
        assert_eq!(spec.meta.model_id, 0x57);
    }

    #[test]
    fn spec_deckt_die_performance_ab() {
        let spec = Spec::load(default_spec_path()).unwrap();
        let pf = &spec.performance;
        assert_eq!(pf.size, 144);
        assert_eq!(pf.part_base + 8 * pf.part_size, 144);
        assert_eq!(pf.part_param.len(), 11); // 12 Byte, Dummy frei
        assert_eq!(pf.param.len(), 28); // 48 − Name 16 − Dummy 1 − 3 Tap-LSB
    }
}
