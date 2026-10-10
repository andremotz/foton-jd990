//! Rohwert → Anzeige, vollständig aus der Spec (ADR-3). Grundlage für jede
//! Oberfläche: das generische UI (M3) und das Panel (M4) zeigen dasselbe.
//!
//! `format` kennt nur den Rohwert. Zwei Anzeigen hängen von einem anderen
//! Parameter ab (in der Spec als `alt` beschrieben) — dafür `format_in`:
//! * Tone Delay Time: bei Delay Mode PLAYMATE 0–254 % statt Sekunden
//! * Reverb Time: bei GATE/REVERSE/FLYING 5–500 ms statt Sekunden

use crate::params::{self, Display, ParamDef, Tone, WAVEFORM_NAMES};
use crate::patch::Patch;

const NOTE_NAMES: [&str; 12] = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];

/// Zahl ohne überflüssige Nachkommastellen; `signed` = Vorzeichen auch bei > 0.
fn number(v: f64, signed: bool) -> String {
    let v = (v * 1000.0).round() / 1000.0;
    let s = if v.fract() == 0.0 { format!("{}", v as i64) } else { format!("{v}") };
    if signed && v > 0.0 { format!("+{s}") } else { s }
}

fn with_unit(s: String, unit: &str) -> String {
    if unit.is_empty() { s } else { format!("{s} {unit}") }
}

/// Anzeige eines Rohwerts. Außerhalb des Spec-Bereichs: `?<rohwert>` —
/// sichtbar statt geklemmt (R7).
pub fn format(def: &ParamDef, raw: u16) -> String {
    if !def.in_range(raw) {
        return format!("?{raw}");
    }
    match def.display {
        Display::Enum => def.values.and_then(|v| v.get(raw as usize)).map_or(format!("?{raw}"), |s| s.to_string()),
        Display::Linear { min, max, unit, extra, labels } => {
            if let Some((_, l)) = labels.iter().find(|(k, _)| *k == raw) {
                return l.to_string();
            }
            let lin_max = def.max - extra.len() as u16;
            if raw > lin_max {
                return extra[(raw - lin_max - 1) as usize].to_string();
            }
            let span = (lin_max - def.min).max(1) as f64;
            let v = min + (raw - def.min) as f64 * (max - min) / span;
            with_unit(number(v, min < 0.0), unit)
        }
        Display::Pan { extra } => match raw {
            0..50 => format!("L{}", 50 - raw),
            50 => "C".into(),
            51..=100 => format!("R{}", raw - 50),
            _ => extra.get((raw - 101) as usize).map_or(format!("?{raw}"), |s| s.to_string()),
        },
        Display::Note => format!("{}{}", NOTE_NAMES[(raw % 12) as usize], raw as i32 / 12 - 1),
        Display::Wave => match WAVEFORM_NAMES.get(raw as usize + 1) {
            Some(name) => format!("{} {name}", raw + 1),
            None => format!("{} (Karte)", raw + 1),
        },
        Display::Table { unit, segments, notes_from, notes } => {
            if let Some(v) = def.values {
                let s = v.get(raw as usize).map_or(format!("?{raw}"), |s| s.to_string());
                return with_unit(s, unit);
            }
            if raw >= notes_from {
                return notes.get((raw - notes_from) as usize).map_or(format!("?{raw}"), |s| s.to_string());
            }
            let Some(&(from, start, step)) = segments.iter().rev().find(|(from, ..)| *from <= raw) else {
                return format!("?{raw}");
            };
            let v = start + (raw - from) as f64 * step;
            let signed = segments.first().is_some_and(|s| s.1 < 0.0);
            match unit {
                "Hz" if v >= 1000.0 => format!("{} kHz", number(v / 1000.0, false)),
                "ms" if v >= 1000.0 => format!("{} s", number(v / 1000.0, false)),
                _ => with_unit(number(v, signed), unit),
            }
        }
    }
}

/// Anzeige im Kontext des Patches — berücksichtigt modusabhängige Anzeigen.
pub fn format_in(patch: &Patch, def: &ParamDef, tone: Option<Tone>) -> String {
    let raw = patch.get(def, tone);
    if !def.in_range(raw) {
        return format!("?{raw}");
    }
    match def.id {
        "tone.wg.delay_time" => {
            let mode = params::find("tone.wg.delay_mode").unwrap();
            if patch.get(mode, tone) == 4 {
                return format!("{} %", raw * 2); // PLAYMATE
            }
        }
        "patch.fx.rv_time" => {
            let ty = params::find("patch.fx.rv_type").unwrap();
            if patch.get(ty, None) >= 6 {
                return format!("{} ms", 5 + raw * 5); // GATE, REVERSE, FLYING1/2
            }
        }
        _ => {}
    }
    format(def, raw)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::params::find;

    fn f(id: &str, raw: u16) -> String {
        format(find(id).unwrap(), raw)
    }

    #[test]
    fn linear() {
        assert_eq!(f("tone.wg.pitch_coarse", 48), "0 st");
        assert_eq!(f("tone.wg.pitch_coarse", 60), "+12 st");
        assert_eq!(f("tone.wg.pitch_coarse", 0), "-48 st");
        assert_eq!(f("tone.tvf.cutoff", 100), "100");
        assert_eq!(f("tone.wg.env_depth", 24), "+12");
        assert_eq!(f("patch.fx.ph_rate", 0), "0.1 Hz");
        assert_eq!(f("patch.fx.ph_rate", 99), "10 Hz");
        assert_eq!(f("patch.fx.ch_feedback", 0), "-98 %");
        assert_eq!(f("patch.fx.ch_feedback", 49), "0 %");
    }

    #[test]
    fn sonderwerte() {
        assert_eq!(f("tone.wg.fxm_depth", 0), "OFF");
        assert_eq!(f("tone.wg.fxm_depth", 1), "1");
        assert_eq!(f("tone.lfo1.delay", 100), "100");
        assert_eq!(f("tone.lfo1.delay", 101), "REL");
        assert_eq!(f("tone.tva.pan", 0), "L50");
        assert_eq!(f("tone.tva.pan", 50), "C");
        assert_eq!(f("tone.tva.pan", 103), "ALT-R");
    }

    #[test]
    fn tabellen() {
        assert_eq!(f("tone.wg.key_follow", 12), "+100 %");
        assert_eq!(f("tone.tvf.key_follow", 0), "-100 %");
        assert_eq!(f("tone.tvf.key_follow", 10), "0 %");
        assert_eq!(f("tone.tvf.key_follow", 40), "+150 %");
        assert_eq!(f("tone.wg.delay_time", 100), "1 s");
        assert_eq!(f("tone.wg.delay_time", 127), "5 s");
        assert_eq!(f("patch.fx.ph_manual", 26), "320 Hz");
        assert_eq!(f("patch.fx.ph_manual", 99), "15 kHz");
        assert_eq!(f("patch.fx.dl_center", 245), "3 s");
        assert_eq!(f("patch.fx.dl_center", 255), "whole");
        assert_eq!(f("patch.fx.rv_time", 99), "20 s");
    }

    #[test]
    fn enums_noten_wellen() {
        assert_eq!(f("tone.tvf.filter_mode", 2), "LPF");
        assert_eq!(f("tone.cs1.dest_1", 4), "P-LFO1");
        assert_eq!(f("patch.key_lo_a", 0), "C-1");
        assert_eq!(f("patch.key_hi_a", 127), "G9");
        assert_eq!(f("patch.key_lo_a", 60), "C4");
        assert_eq!(f("tone.wg.waveform", 0), "1 Syn Saw 1");
        assert_eq!(f("tone.wg.waveform", 194), "195 Loop 4");
        assert_eq!(f("tone.wg.waveform", 200), "201 (Karte)");
    }

    #[test]
    fn modusabhaengige_anzeigen() {
        let mut p = Patch::from_bytes([0; params::PATCH_SIZE]);
        let (mode, time) = (find("tone.wg.delay_mode").unwrap(), find("tone.wg.delay_time").unwrap());
        p.set(time, Some(Tone::B), 127).unwrap();
        assert_eq!(format_in(&p, time, Some(Tone::B)), "5 s");
        p.set(mode, Some(Tone::B), 4).unwrap();
        assert_eq!(format_in(&p, time, Some(Tone::B)), "254 %");
        let (ty, rt) = (find("patch.fx.rv_type").unwrap(), find("patch.fx.rv_time").unwrap());
        p.set(rt, None, 99).unwrap();
        assert_eq!(format_in(&p, rt, None), "20 s");
        p.set(ty, None, 6).unwrap(); // GATE
        assert_eq!(format_in(&p, rt, None), "500 ms");
    }

    #[test]
    fn ausserhalb_bleibt_sichtbar() {
        assert_eq!(f("tone.tvf.cutoff", 127), "?127");
    }

    #[test]
    fn jeder_wert_jedes_parameters_ist_formatierbar() {
        for def in crate::params::PATCH_PARAMS.iter().chain(crate::params::TONE_PARAMS) {
            for raw in def.min..=def.max {
                let s = format(def, raw);
                assert!(!s.starts_with('?'), "{} = {raw} → {s}", def.id);
            }
        }
    }
}
