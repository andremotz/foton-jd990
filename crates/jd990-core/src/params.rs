//! Parametertabellen — generiert aus `spec/params.toml`.

/// Wie ein Rohwert angezeigt wird — generiert aus `display` in der Spec.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Display {
    /// raw.min…(raw.max − extra) ↦ min…max; darüber `extra`; `labels` übersteuern einzelne Rohwerte
    Linear { min: f64, max: f64, unit: &'static str, extra: &'static [&'static str], labels: &'static [(u16, &'static str)] },
    /// Name je Rohwert steht in `ParamDef::values`
    Enum,
    /// Werteliste (`ParamDef::values`) oder Segmente (from, start, step) + Notenwerte ab `notes_from`
    Table { unit: &'static str, segments: &'static [(u16, f64, f64)], notes_from: u16, notes: &'static [&'static str] },
    /// 0…100 = L50…R50, darüber `extra`
    Pan { extra: &'static [&'static str] },
    /// MIDI-Note, 0 = C-1
    Note,
    /// Wellenform: Nummer = Rohwert + 1, Name aus `WAVEFORM_NAMES[rohwert + 1]`
    Wave,
}

/// Beschreibung eines Parameters, direkt aus der Spec.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ParamDef {
    pub id: &'static str,
    pub label: &'static str,
    pub group: &'static str,
    /// Byte-Offset im Block (Patch Common: 0–117, Tone: 0–91).
    pub offset: usize,
    /// 1, oder 2 für MSB/LSB-Paare (Wert = MSB·128 + LSB).
    pub bytes: usize,
    pub min: u16,
    pub max: u16,
    pub verified: bool,
    /// Anzeigenamen je Rohwert, falls Enum oder Werteliste.
    pub values: Option<&'static [&'static str]>,
    pub display: Display,
    /// Position der Gruppe in `group_order` der Spec — Reihenfolge im UI
    /// (unabhängig von den Byte-Offsets, ADR-3 / UI-Konzept Prinzip 6).
    pub group_rank: u16,
    /// Rohwerte, die die UI sperrt (out of scope, z. B. Kartenquellen) — der
    /// Codec reicht sie trotzdem unverändert durch (R7).
    pub locked: &'static [u16],
    /// Wert im Init-Patch (Doppelklick-Ziel); bei Tone-Parametern für alle Tones gleich.
    pub init: u16,
}

impl ParamDef {
    pub fn in_range(&self, raw: u16) -> bool {
        (self.min..=self.max).contains(&raw)
    }
}

/// Einer der vier Tones eines Patches.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Tone {
    A = 0,
    B = 1,
    C = 2,
    D = 3,
}

impl Tone {
    pub const ALL: [Tone; 4] = [Tone::A, Tone::B, Tone::C, Tone::D];

    pub fn base(self) -> usize {
        TONE_BASES[self as usize]
    }

    pub fn letter(self) -> char {
        (b'A' + self as u8) as char
    }
}

include!(concat!(env!("OUT_DIR"), "/params.rs"));

/// Sucht einen Performance- oder Part-Parameter anhand seiner ID.
pub fn find_perf(id: &str) -> Option<&'static ParamDef> {
    PERF_PARAMS.iter().chain(PART_PARAMS).find(|p| p.id == id)
}

/// Sucht einen Patch- oder Tone-Parameter anhand seiner ID.
pub fn find(id: &str) -> Option<&'static ParamDef> {
    PATCH_PARAMS.iter().chain(TONE_PARAMS).find(|p| p.id == id)
}
