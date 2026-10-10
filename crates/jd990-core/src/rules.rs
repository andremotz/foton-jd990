//! Regeln, die das Gerät selbst beim Empfang eines Werts anwendet.
//!
//! Der Editor muss sie nachbilden — sonst weicht seine Sicht stillschweigend vom
//! Gerät ab. Jede Regel hier ist an der Hardware beobachtet (HIL-Sweep, ADR-6)
//! und im Logbuch belegt; nichts davon ist aus dem Handbuch abgeleitet.

use crate::params::{self, ParamDef, Tone};
use crate::patch::Patch;

/// Wendet die Geräteregeln an, nachdem `def` (ggf. für `tone`) gesetzt wurde.
/// Gibt die Offsets zurück, die das Gerät zusätzlich ändert.
pub fn apply_after_set(patch: &mut Patch, def: &ParamDef, _tone: Option<Tone>) -> Vec<usize> {
    let mut changed = Vec::new();
    // Key Range (Logbuch 2026-09-28): Low > High → High zieht mit; High < Low → Low zieht mit.
    for t in ['a', 'b', 'c', 'd'] {
        let lo = params::find(&format!("patch.key_lo_{t}")).unwrap();
        let hi = params::find(&format!("patch.key_hi_{t}")).unwrap();
        let (l, h) = (patch.get(lo, None), patch.get(hi, None));
        if l <= h {
            continue;
        }
        if def.id == lo.id {
            patch.set(hi, None, l).unwrap();
            changed.push(hi.offset);
        } else if def.id == hi.id {
            patch.set(lo, None, h).unwrap();
            changed.push(lo.offset);
        }
    }
    changed
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::params::find;

    #[test]
    fn key_range_zieht_in_beide_richtungen_mit() {
        let mut p = Patch::from_bytes([0; params::PATCH_SIZE]);
        let (lo, hi) = (find("patch.key_lo_b").unwrap(), find("patch.key_hi_b").unwrap());
        p.set(hi, None, 100).unwrap();
        p.set(lo, None, 110).unwrap();
        assert_eq!(apply_after_set(&mut p, lo, None), vec![hi.offset]);
        assert_eq!(p.get(hi, None), 110);
        p.set(hi, None, 105).unwrap();
        assert_eq!(apply_after_set(&mut p, hi, None), vec![lo.offset]);
        assert_eq!(p.get(lo, None), 105);
        // gültiger Zustand → keine Änderung
        assert!(apply_after_set(&mut p, lo, None).is_empty());
    }
}
