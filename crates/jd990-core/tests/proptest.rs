//! ADR-6, Ebene 1b: zufälliger gültiger Patch → encode → decode → identisch.

use jd990_core::params::{PATCH_PARAMS, TONE_PARAMS};
use jd990_core::sysex::Address;
use jd990_core::{Patch, Tone};
use proptest::prelude::*;

/// Ein gültiger Patch: Name aus druckbarem ASCII, jeder Parameter im Spec-Bereich.
fn valid_patch() -> impl Strategy<Value = Patch> {
    let n = PATCH_PARAMS.len() + 4 * TONE_PARAMS.len();
    (proptest::collection::vec(0x20u8..0x7F, 16), proptest::collection::vec(any::<u16>(), n)).prop_map(|(name, seeds)| {
        let mut raw = [0u8; 486];
        raw[..16].copy_from_slice(&name);
        let mut p = Patch::from_bytes(raw);
        let mut s = seeds.into_iter();
        for def in PATCH_PARAMS {
            let v = def.min + s.next().unwrap() % (def.max - def.min + 1);
            p.set(def, None, v).unwrap();
        }
        for t in Tone::ALL {
            for def in TONE_PARAMS {
                let v = def.min + s.next().unwrap() % (def.max - def.min + 1);
                p.set(def, Some(t), v).unwrap();
            }
        }
        p
    })
}

proptest! {
    #[test]
    fn encode_decode_ist_identitaet(p in valid_patch(), n in 0u8..64) {
        for base in [Address::TEMP_PATCH, Address::internal_patch(n)] {
            let (back, at) = Patch::from_syx(&p.to_syx(0x10, base)).unwrap();
            prop_assert_eq!(at, base);
            prop_assert_eq!(back.as_bytes(), p.as_bytes());
            prop_assert!(back.out_of_range().is_empty());
        }
    }

    #[test]
    fn jede_nachricht_bleibt_7_bit_und_max_256(p in valid_patch()) {
        for m in p.to_dt1(0x10, Address::TEMP_PATCH) {
            prop_assert!(m.data.len() <= 256);
            prop_assert!(m.encode()[1..m.encode().len() - 1].iter().all(|&b| b < 0x80));
        }
    }
}
