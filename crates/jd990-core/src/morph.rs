//! Morph und Variation (M5, Meilensteinplan „Randomize (gewichtet, nicht
//! uniform), Morph zwischen zwei Patches").
//!
//! Beides arbeitet auf Rohwerten und bleibt immer im Spec-Bereich. Auswahlwerte
//! (Enums, Tabellen mit wenigen Einträgen, Wellenform) werden nicht
//! interpoliert, sondern umgeschaltet — ein „halber Filtertyp“ existiert nicht.

use crate::params::{self, Display, ParamDef, Tone};
use crate::patch::Patch;

/// Auswahlwert? Dann nicht interpolieren, sondern schalten.
fn is_choice(def: &ParamDef) -> bool {
    match def.display {
        Display::Enum | Display::Wave => true,
        Display::Table { .. } => def.values.is_some() && def.max - def.min < 48,
        _ => false,
    }
}

/// Gesperrte Werte (z. B. Kartenquellen) nie erzeugen.
fn allowed(def: &ParamDef, v: u16) -> bool {
    def.in_range(v) && !def.locked.contains(&v)
}

fn each_param(mut f: impl FnMut(&'static ParamDef, Option<Tone>)) {
    for d in params::PATCH_PARAMS {
        f(d, None);
    }
    for t in Tone::ALL {
        for d in params::TONE_PARAMS {
            f(d, Some(t));
        }
    }
}

/// Abstand eines Parameters 0…1: Auswahlwerte gleich/ungleich, sonst Anteil am Bereich.
fn distance(def: &ParamDef, x: u16, y: u16) -> f32 {
    if x == y {
        0.0
    } else if is_choice(def) {
        1.0
    } else {
        (x as f32 - y as f32).abs() / (def.max.saturating_sub(def.min)).max(1) as f32
    }
}

/// Gewicht eines Tone-Parameters für die Ähnlichkeit: was man am deutlichsten hört, zählt mehr.
fn tone_weight(def: &ParamDef) -> f32 {
    let id = def.id;
    if matches!(def.display, Display::Wave) {
        8.0
    } else if id.ends_with("tvf.cutoff") || id.ends_with("wg.coarse") {
        3.0
    } else if id.ends_with("tvf.resonance") || id.ends_with("tvf.filter_mode") || id.ends_with("tva.level") {
        2.0
    } else if id.contains("_env.time_") || id.contains("_env.level_") || id.contains("_env.sustain") || id.contains("depth_pitch") {
        1.0
    } else {
        0.3
    }
}

fn tone_distance(a: &Patch, ta: Tone, b: &Patch, tb: Tone) -> f32 {
    let (mut sum, mut weight) = (0.0f32, 0.0f32);
    for d in params::TONE_PARAMS {
        let w = tone_weight(d);
        sum += w * distance(d, a.get(d, Some(ta)), b.get(d, Some(tb)));
        weight += w;
    }
    sum / weight
}

/// Klangähnlichkeit zweier Patches, 0 … 1 (1 = gleich) — für „Ähnliche Patches“.
///
/// Gewichtet nach dem, was man hört: Es zählen nur die klingenden Tones (Layer),
/// und die werden bestmöglich einander zugeordnet (Tone A des einen darf Tone C des
/// anderen entsprechen). Ein Tone ohne Gegenstück zählt als ganz verschieden.
/// Wellenform, Cutoff und Tonhöhe wiegen am meisten; Patch-Parameter (Effekte,
/// Key FX …) machen 15 % aus. Der Name spielt keine Rolle.
pub fn similarity(a: &Patch, b: &Patch) -> f32 {
    let mask = |p: &Patch| params::find("patch.layer_tone").map(|d| p.get(d, None)).unwrap_or(15);
    let (ma, mb) = (mask(a), mask(b));
    let on = |m: u16, i: usize| (m >> i) & 1 == 1;

    // Tones: beste Zuordnung über alle 24 Permutationen
    let mut dist = [[0.0f32; 4]; 4];
    for (row, ta) in dist.iter_mut().zip(Tone::ALL) {
        for (cell, tb) in row.iter_mut().zip(Tone::ALL) {
            *cell = tone_distance(a, ta, b, tb);
        }
    }
    let count = (0..4).filter(|&i| on(ma, i)).count().max((0..4).filter(|&j| on(mb, j)).count());
    let mut best = f32::MAX;
    let mut perm = [0usize, 1, 2, 3];
    permute(&mut perm, 0, &mut |p| {
        let mut cost = 0.0;
        for (i, &j) in p.iter().enumerate() {
            match (on(ma, i), on(mb, j)) {
                (true, true) => cost += dist[i][j],
                (true, false) | (false, true) => cost += 1.0,
                (false, false) => {}
            }
        }
        best = best.min(cost);
    });
    let tones = if count == 0 { 0.0 } else { best / count as f32 };

    let (mut sum, mut weight) = (0.0f32, 0.0f32);
    for d in params::PATCH_PARAMS {
        if d.id == "patch.active_tone" {
            continue; // nur Bearbeitungsauswahl, klingt nicht
        }
        sum += distance(d, a.get(d, None), b.get(d, None));
        weight += 1.0;
    }
    let patch = if weight == 0.0 { 0.0 } else { sum / weight };
    (1.0 - (0.85 * tones + 0.15 * patch)).clamp(0.0, 1.0)
}

fn permute(p: &mut [usize; 4], k: usize, f: &mut impl FnMut(&[usize; 4])) {
    if k == p.len() {
        f(p);
        return;
    }
    for i in k..p.len() {
        p.swap(k, i);
        permute(p, k + 1, f);
        p.swap(k, i);
    }
}

/// Morph: `t` = 0 → `a`, 1 → `b`. Kontinuierliche Werte linear, Auswahlwerte
/// schalten bei 0,5 um. Name: von `a` bis 0,5, danach von `b`.
pub fn morph(a: &Patch, b: &Patch, t: f32) -> Patch {
    let t = t.clamp(0.0, 1.0);
    let mut out = if t < 0.5 { a.clone() } else { b.clone() };
    each_param(|def, tone| {
        let (x, y) = (a.get(def, tone), b.get(def, tone));
        let v = if is_choice(def) {
            if t < 0.5 { x } else { y }
        } else {
            (x as f32 + (y as f32 - x as f32) * t).round() as u16
        };
        if allowed(def, v) {
            let _ = out.set(def, tone, v);
        }
    });
    out
}

/// Deterministischer Zufallsgenerator (PCG-artig), damit Varianten reproduzierbar sind.
struct Rng(u64);
impl Rng {
    fn next(&mut self) -> f32 {
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        ((self.0 >> 40) as f32) / ((1u64 << 24) as f32)
    }
}

/// Variation: gewichtete Zufallsänderung rund um `base`. `amount` 0…1 ist die
/// maximale Auslenkung als Anteil des Bereichs; Auswahlwerte wechseln nur mit
/// Wahrscheinlichkeit `amount / 4` (selten, weil sie den Klang sprunghaft ändern).
/// Nicht verändert werden Name, Tone-Schalter/Layer, Structure, Wellenform,
/// Key/Velocity-Bereiche und die Pegel, damit die Variante spielbar und erkennbar bleibt.
pub fn variation(base: &Patch, amount: f32, seed: u64) -> Patch {
    let amount = amount.clamp(0.0, 1.0);
    let mut rng = Rng(seed ^ 0x4A44_3939_3056_4152);
    let mut out = base.clone();
    // Wellenform bleibt: ein Wechsel (z. B. Bass → Conga) ist kein Variieren, sondern ein anderer Klang
    const KEEP: [&str; 11] = [
        "patch.layer_tone", "patch.active_tone", "patch.structure_ab", "patch.structure_cd",
        "patch.level", "patch.octave", "tone.wg.wave_source", "tone.wg.waveform", "tone.tva.level", "patch.solo_sw", "patch.porta_sw",
    ];
    each_param(|def, tone| {
        if KEEP.contains(&def.id) || def.group == "KEY RANGE" || def.group == "VELOCITY" {
            return;
        }
        let x = base.get(def, tone);
        let r = rng.next();
        let v = if is_choice(def) {
            if r < amount / 4.0 {
                let span = (def.max - def.min + 1) as f32;
                def.min + ((rng.next() * span) as u16).min(def.max - def.min)
            } else {
                x
            }
        } else {
            // Dreieckverteilung um den Ausgangswert: kleine Änderungen häufiger
            let tri = (r + rng.next()) - 1.0; // −1 … +1
            let range = (def.max - def.min) as f32;
            (x as f32 + tri * amount * range).round().clamp(def.min as f32, def.max as f32) as u16
        };
        if allowed(def, v) {
            let _ = out.set(def, tone, v);
        }
    });
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::params::find;

    fn two() -> (Patch, Patch) {
        let a = Patch::init();
        let mut b = Patch::init();
        b.set(find("tone.tvf.cutoff").unwrap(), Some(Tone::A), 0).unwrap();
        b.set(find("tone.tvf.filter_mode").unwrap(), Some(Tone::A), 0).unwrap();
        (a, b)
    }

    #[test]
    fn aehnlichkeit_gleich_nah_fern() {
        let (a, b) = two();
        assert_eq!(similarity(&a, &a), 1.0);
        let near = morph(&a, &b, 0.1);
        let s_near = similarity(&a, &near);
        let s_far = similarity(&a, &b);
        assert!(s_near > s_far, "nah {s_near} fern {s_far}");
        assert!((0.0..1.0).contains(&s_far));
        assert!((similarity(&a, &b) - similarity(&b, &a)).abs() < 1e-5); // symmetrisch
    }

    #[test]
    fn aehnlichkeit_ignoriert_tone_reihenfolge() {
        let (a, _) = two();
        let layer = find("patch.layer_tone").unwrap();
        let mut moved = a.clone();
        moved.swap_tones(Tone::A, Tone::C);
        // Tone A klingt jetzt als C: Layer entsprechend umsetzen
        let m = a.get(layer, None);
        let m2 = (m & !0b0101) | ((m & 1) << 2) | ((m >> 2) & 1);
        moved.set(layer, None, m2).unwrap();
        assert!(similarity(&a, &moved) > 0.97, "{}", similarity(&a, &moved));
    }

    #[test]
    fn morph_endpunkte_und_mitte() {
        let (a, b) = two();
        assert_eq!(morph(&a, &b, 0.0), a);
        assert_eq!(morph(&a, &b, 1.0), b);
        let m = morph(&a, &b, 0.25);
        assert_eq!(m.get(find("tone.tvf.cutoff").unwrap(), Some(Tone::A)), 75);
        assert_eq!(m.get(find("tone.tvf.filter_mode").unwrap(), Some(Tone::A)), 2); // schaltet erst bei 0,5
        assert!(m.out_of_range().is_empty());
    }

    #[test]
    fn variation_bleibt_im_bereich_und_ist_reproduzierbar() {
        let base = Patch::init();
        for seed in 0..50 {
            let v = variation(&base, 0.5, seed);
            assert!(v.out_of_range().is_empty(), "seed {seed}: {:?}", v.out_of_range());
            assert_eq!(v.get(find("patch.active_tone").unwrap(), None), 1);
            assert_eq!(v.get(find("tone.wg.wave_source").unwrap(), Some(Tone::B)), 0); // nie Karte
            assert_eq!(v.get(find("tone.wg.waveform").unwrap(), Some(Tone::C)), base.get(find("tone.wg.waveform").unwrap(), Some(Tone::C)));
            assert_eq!(v.name(), base.name());
        }
        assert_eq!(variation(&base, 0.3, 7), variation(&base, 0.3, 7));
        assert_ne!(variation(&base, 0.3, 7), variation(&base, 0.3, 8));
        assert_eq!(variation(&base, 0.0, 7), base);
    }
}
