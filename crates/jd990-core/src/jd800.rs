//! JD-800-Patches lesen und in JD-990-Patches umwandeln (M5, UI-Konzept/ADR-5).
//!
//! Struktur (384 Byte, Model-ID 3DH, 3-Byte-Adressen) und Umrechnung portiert aus
//! JDTools — JD-800.hpp und Convert800to990.cpp
//! © 2022–2024 Johannes Schultz, BSD 3-clause, https://github.com/sagamusix/JDTools
//!
//! Der JD-990 kennt alles, was der JD-800 kann; die 990-Extras (Structure,
//! Velocity-Ranges, FXM, Pan …) werden neutral gesetzt. Hardware-Controller des
//! JD-800 (Aftertouch-Bend, Lever, Aftertouch-Mod) werden auf die Control Sources
//! des 990 abgebildet — wie in JDTools.

use crate::params::{self, Tone};
use crate::patch::Patch;
use crate::sysex::split_messages;

pub const MODEL_ID_800: u8 = 0x3D;
pub const PATCH_SIZE_800: usize = 384;
const TONE_BASE_800: usize = 96;
const TONE_SIZE_800: usize = 72;
const BASE_TEMP: u32 = 0x00 << 14;
const BASE_INTERNAL: u32 = 0x05 << 14;

/// Findet JD-800-Patches (Temp und Internal I:11 … I:88) in einem Bytestrom.
/// Ergebnis: (384 Byte, Bezeichnung des Platzes). Prüfsummen werden geprüft.
pub fn patches_from_bytes(bytes: &[u8]) -> Vec<([u8; PATCH_SIZE_800], String)> {
    let owned;
    let bytes = if crate::smf::is_smf(bytes) {
        match crate::smf::extract_sysex(bytes) {
            Ok(b) => { owned = b; &owned[..] }
            Err(_) => return vec![],
        }
    } else {
        bytes
    };
    // Speicherabbild aus allen DT1 an Model 3DH
    let mut mem: std::collections::BTreeMap<u32, u8> = Default::default();
    for m in split_messages(bytes) {
        if m.len() < 10 || m[1] != 0x41 || m[3] != MODEL_ID_800 || m[4] != 0x12 {
            continue;
        }
        let body = &m[5..m.len() - 2];
        if crate::sysex::checksum(body) != m[m.len() - 2] {
            continue;
        }
        let addr = (m[5] as u32) << 14 | (m[6] as u32) << 7 | m[7] as u32;
        for (i, &b) in m[8..m.len() - 2].iter().enumerate() {
            mem.insert(addr + i as u32, b);
        }
    }
    let take = |base: u32| -> Option<[u8; PATCH_SIZE_800]> {
        let mut p = [0u8; PATCH_SIZE_800];
        for (i, b) in p.iter_mut().enumerate() {
            *b = *mem.get(&(base + i as u32))?;
        }
        Some(p)
    };
    let mut out = Vec::new();
    if let Some(p) = take(BASE_TEMP) {
        out.push((p, "JD-800 Temp".to_string()));
    }
    for n in 0..64u32 {
        if let Some(p) = take(BASE_INTERNAL + n * PATCH_SIZE_800 as u32) {
            out.push((p, format!("JD-800 I:{}{}", n / 8 + 1, n % 8 + 1)));
        }
    }
    out
}

/// JD-800-Patch → JD-990-Patch (Umrechnung nach JDTools ConvertPatch800To990).
pub fn convert(p8: &[u8; PATCH_SIZE_800]) -> Patch {
    let mut p = Patch::init();
    let c = |o: usize| p8[o] as u16;

    // Name
    for (i, &b) in p8[0..16].iter().enumerate() {
        set_name_byte(&mut p, i, b);
    }
    // JD-800-Werte liegen in den 990-Bereichen; falls nicht, unverändert übernehmen (R7)
    let mut set = |id: &str, tone: Option<Tone>, v: u16| {
        let def = params::find(id).unwrap_or_else(|| panic!("Spec kennt {id} nicht"));
        if p.set(def, tone, v).is_err() {
            p.set_unchecked(def, tone, v);
        }
    };

    // Common
    set("patch.level", None, c(16));
    set("patch.pan", None, 50);
    set("patch.analog_feel", None, 0);
    set("patch.voice_prio", None, 0);
    set("patch.bend_down", None, c(25));
    set("patch.bend_up", None, c(26));
    set("patch.tone_ctrl_1", None, 0);
    set("patch.tone_ctrl_2", None, 1);
    set("patch.layer_tone", None, c(33));
    set("patch.active_tone", None, c(34));
    set("patch.porta_sw", None, c(30));
    set("patch.porta_mode", None, c(31));
    set("patch.porta_type", None, 1);
    set("patch.porta_time", None, c(32));
    set("patch.solo_sw", None, c(28));
    set("patch.solo_legato", None, c(29));
    set("patch.solo_sync", None, 0);
    for (i, id) in ["low_freq", "low_gain", "mid_freq", "mid_q", "mid_gain", "high_freq", "high_gain"].iter().enumerate() {
        set(&format!("patch.eq.{id}"), None, c(35 + i));
    }
    set("patch.structure_ab", None, 0);
    set("patch.structure_cd", None, 0);
    for (i, t) in ["a", "b", "c", "d"].iter().enumerate() {
        set(&format!("patch.key_lo_{t}"), None, c(17 + 2 * i));
        set(&format!("patch.key_hi_{t}"), None, c(18 + 2 * i));
        set(&format!("patch.velo_range_{t}"), None, 0);
        set(&format!("patch.velo_point_{t}"), None, 64);
        set(&format!("patch.velo_fade_{t}"), None, 0);
    }

    // Effekte
    set("patch.fx.balance_b", None, c(59));
    for id in ["ctrl_src_1", "ctrl_dst_1", "ctrl_src_2", "ctrl_dst_2"] {
        set(&format!("patch.fx.{id}"), None, 0);
    }
    set("patch.fx.ctrl_dep_1", None, 50);
    set("patch.fx.ctrl_dep_2", None, 50);
    set("patch.fx.a_sequence", None, c(50));
    set("patch.fx.b_sequence", None, c(51));
    for i in 0..4 { set(&format!("patch.fx.a_sw_{}", i + 1), None, c(52 + i)); }
    for i in 0..3 { set(&format!("patch.fx.b_sw_{}", i + 1), None, c(56 + i)); }
    let fx: [(&str, usize); 25] = [
        ("ds_type", 60), ("ds_drive", 61), ("ds_level", 62),
        ("ph_manual", 63), ("ph_rate", 64), ("ph_depth", 65), ("ph_reso", 66), ("ph_mix", 67),
        ("sp_band_1", 68), ("sp_band_2", 69), ("sp_band_3", 70), ("sp_band_4", 71), ("sp_band_5", 72), ("sp_band_6", 73), ("sp_width", 74),
        ("en_sens", 75), ("en_mix", 76),
        ("dl_center", 77), ("dl_center_lv", 78), ("dl_left", 79), ("dl_left_lv", 80), ("dl_right", 81), ("dl_right_lv", 82), ("dl_feedback", 83),
        ("ch_rate", 84),
    ];
    for (id, o) in fx { set(&format!("patch.fx.{id}"), None, c(o)); }
    set("patch.fx.dl_mode", None, 0);
    for (id, o) in [("ch_depth", 85), ("ch_delay", 86), ("ch_feedback", 87), ("ch_level", 88),
                    ("rv_type", 89), ("rv_predelay", 90), ("rv_early", 91), ("rv_hf_damp", 92), ("rv_time", 93), ("rv_level", 94)] {
        set(&format!("patch.fx.{id}"), None, c(o));
    }
    set("patch.octave", None, 1);

    // Tones
    let a_touch_bend = c(27);
    for (ti, tone) in Tone::ALL.iter().enumerate() {
        let b = TONE_BASE_800 + ti * TONE_SIZE_800;
        let t = |o: usize| p8[b + o] as u16;
        let s = Some(*tone);
        let copy: [(&str, usize); 48] = [
            ("wg.wave_source", 14), ("wg.pitch_coarse", 17), ("wg.pitch_fine", 18), ("wg.pitch_random", 19),
            ("wg.key_follow", 20), ("wg.bender_switch", 21),
            ("pitch_env.velo", 27), ("pitch_env.time_velo", 28), ("pitch_env.time_kf", 29), ("pitch_env.level_0", 30),
            ("pitch_env.time_1", 31), ("pitch_env.level_1", 32), ("pitch_env.time_2", 33), ("pitch_env.time_3", 34),
            ("pitch_env.level_3", 35),
            ("tvf.filter_mode", 36), ("tvf.cutoff", 37), ("tvf.resonance", 38), ("tvf.key_follow", 39), ("tvf.env_depth", 43),
            ("tvf_env.velo", 44), ("tvf_env.time_velo", 45), ("tvf_env.time_kf", 46), ("tvf_env.time_1", 47), ("tvf_env.level_1", 48),
            ("tvf_env.time_2", 49), ("tvf_env.level_2", 50), ("tvf_env.time_3", 51), ("tvf_env.sustain", 52), ("tvf_env.time_4", 53),
            ("tvf_env.level_4", 54),
            ("tva.bias_direction", 55), ("tva.bias_point", 56), ("tva.bias_level", 57), ("tva.level", 58),
            ("tva_env.velo", 62), ("tva_env.time_velo", 63), ("tva_env.time_kf", 64), ("tva_env.time_1", 65), ("tva_env.level_1", 66),
            ("tva_env.time_2", 67), ("tva_env.level_2", 68), ("tva_env.time_3", 69), ("tva_env.sustain", 70), ("tva_env.time_4", 71),
            ("common.velocity_curve", 0), ("common.hold_control", 1), ("lfo1.rate", 2),
        ];
        for (id, o) in copy { set(&format!("tone.{id}"), s, t(o)); }
        set("tone.wg.waveform", s, t(15) << 7 | t(16));
        set("tone.wg.fxm_color", s, 0);
        set("tone.wg.fxm_depth", s, 0);
        set("tone.wg.sync_slave", s, 0);
        set("tone.wg.delay_mode", s, 0);
        set("tone.wg.delay_time", s, 0);
        set("tone.wg.env_depth", s, 24);
        set("tone.pitch_env.sustain", s, 50);
        set("tone.tva.pan", s, 50);
        set("tone.tva.pan_key_follow", s, 7);

        // LFO: JD-800 TRI, SAW, SQU, S&H, RND → 990-Indizes
        const WAVE: [u16; 5] = [0, 2, 3, 5, 6];
        for (lfo, base, sens) in [("lfo1", 2, 23), ("lfo2", 8, 24)] {
            set(&format!("tone.{lfo}.rate"), s, t(base));
            set(&format!("tone.{lfo}.delay"), s, t(base + 1));
            set(&format!("tone.{lfo}.fade"), s, t(base + 2));
            set(&format!("tone.{lfo}.waveform"), s, WAVE.get(t(base + 3) as usize).copied().unwrap_or(0));
            set(&format!("tone.{lfo}.offset"), s, t(base + 4));
            set(&format!("tone.{lfo}.key_trigger"), s, t(base + 5));
            set(&format!("tone.{lfo}.depth_pitch"), s, t(sens));
            let sel = if lfo == "lfo1" { 0 } else { 1 };
            set(&format!("tone.{lfo}.depth_tvf"), s, if t(41) == sel { t(42) } else { 50 });
            set(&format!("tone.{lfo}.depth_tva"), s, if t(60) == sel { t(61) } else { 50 });
        }

        // Control Source 1 = Mod-Rad (Lever): Pitch über LFO
        let lever = t(25);
        set("tone.cs1.dest_1", s, if lever < 50 { 5 } else { 4 });
        set("tone.cs1.depth_1", s, if lever < 50 { 100 - lever } else { lever });
        for (i, d) in [(2, 6), (3, 8), (4, 10)] {
            set(&format!("tone.cs1.dest_{i}"), s, d);
            set(&format!("tone.cs1.depth_{i}"), s, 50);
        }
        // Control Source 2 = Aftertouch: Pitch-Bend, Pitch über LFO, TVF, TVA
        set("tone.cs2.dest_1", s, 0);
        let bend = if t(22) != 0 {
            match a_touch_bend { 0 => 14, 1 => 26, n => 38 + (n - 2) }
        } else {
            50
        };
        set("tone.cs2.depth_1", s, bend);
        let atm = t(26);
        set("tone.cs2.dest_2", s, if atm < 50 { 5 } else { 4 });
        set("tone.cs2.depth_2", s, if atm < 50 { 100 - atm } else { atm });
        set("tone.cs2.dest_3", s, 1);
        set("tone.cs2.depth_3", s, t(40));
        set("tone.cs2.dest_4", s, 3);
        set("tone.cs2.depth_4", s, t(59));
    }
    p
}

fn set_name_byte(p: &mut Patch, i: usize, b: u8) {
    p.set_name_byte(i, if (0x20..0x7F).contains(&b) { b } else { b' ' });
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::params::find;
    use crate::sysex::checksum;

    /// JDTools DEFAULT_PATCH_800 (JDTools.cpp) — © Johannes Schultz, BSD 3-clause
    const DEFAULT_800: [u8; PATCH_SIZE_800] = [
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x64, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F, 0x02, 0x02, 0x1A, 0x00, 0x00, 0x00, 0x00,
        0x32, 0x01, 0x01, 0x01, 0x0F, 0x07, 0x00, 0x0F, 0x00, 0x0F, 0x01, 0x24, 0x01, 0x00, 0x40, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x03, 0x32, 0x46, 0x1C,
        0x13, 0x1E, 0x32, 0x64, 0x05, 0x19, 0x05, 0x19, 0x05, 0x19, 0x02, 0x32, 0x32, 0x6E, 0x32, 0x5F,
        0x32, 0x69, 0x32, 0x4A, 0x02, 0x3C, 0x4F, 0x4A, 0x64, 0x02, 0x1E, 0x32, 0x0C, 0x18, 0x46, 0x00,
        0x02, 0x01, 0x4B, 0x00, 0x00, 0x00, 0x01, 0x01, 0x32, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
        0x00, 0x30, 0x32, 0x00, 0x0C, 0x01, 0x00, 0x32, 0x32, 0x50, 0x32, 0x32, 0x32, 0x0A, 0x32, 0x32,
        0x32, 0x32, 0x32, 0x32, 0x02, 0x64, 0x00, 0x1E, 0x32, 0x00, 0x32, 0x32, 0x32, 0x32, 0x0A, 0x00,
        0x64, 0x32, 0x64, 0x32, 0x64, 0x32, 0x00, 0x00, 0x3C, 0x0A, 0x50, 0x32, 0x01, 0x32, 0x32, 0x32,
        0x0A, 0x00, 0x64, 0x32, 0x64, 0x32, 0x64, 0x32, 0x02, 0x01, 0x4B, 0x00, 0x00, 0x00, 0x01, 0x01,
        0x32, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x30, 0x32, 0x00, 0x0C, 0x01, 0x00, 0x32,
        0x32, 0x50, 0x32, 0x32, 0x32, 0x0A, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x02, 0x64, 0x00, 0x1E,
        0x32, 0x00, 0x32, 0x32, 0x32, 0x32, 0x0A, 0x00, 0x64, 0x32, 0x64, 0x32, 0x64, 0x32, 0x00, 0x00,
        0x3C, 0x0A, 0x50, 0x32, 0x01, 0x32, 0x32, 0x32, 0x0A, 0x00, 0x64, 0x32, 0x64, 0x32, 0x64, 0x32,
        0x02, 0x01, 0x4B, 0x00, 0x00, 0x00, 0x01, 0x01, 0x32, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
        0x00, 0x30, 0x32, 0x00, 0x0C, 0x01, 0x00, 0x32, 0x32, 0x50, 0x32, 0x32, 0x32, 0x0A, 0x32, 0x32,
        0x32, 0x32, 0x32, 0x32, 0x02, 0x64, 0x00, 0x1E, 0x32, 0x00, 0x32, 0x32, 0x32, 0x32, 0x0A, 0x00,
        0x64, 0x32, 0x64, 0x32, 0x64, 0x32, 0x00, 0x00, 0x3C, 0x0A, 0x50, 0x32, 0x01, 0x32, 0x32, 0x32,
        0x0A, 0x00, 0x64, 0x32, 0x64, 0x32, 0x64, 0x32, 0x02, 0x01, 0x4B, 0x00, 0x00, 0x00, 0x01, 0x01,
        0x32, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x30, 0x32, 0x00, 0x0C, 0x01, 0x00, 0x32,
        0x32, 0x50, 0x32, 0x32, 0x32, 0x0A, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x02, 0x64, 0x00, 0x1E,
        0x32, 0x00, 0x32, 0x32, 0x32, 0x32, 0x0A, 0x00, 0x64, 0x32, 0x64, 0x32, 0x64, 0x32, 0x00, 0x00,
        0x3C, 0x0A, 0x50, 0x32, 0x01, 0x32, 0x32, 0x32, 0x0A, 0x00, 0x64, 0x32, 0x64, 0x32, 0x64, 0x32,
    ];

    fn as_syx(p: &[u8; PATCH_SIZE_800], base: u32) -> Vec<u8> {
        let mut out = Vec::new();
        for (i, chunk) in p.chunks(256).enumerate() {
            let a = base + (i * 256) as u32;
            let mut body = vec![(a >> 14) as u8 & 0x7F, (a >> 7) as u8 & 0x7F, a as u8 & 0x7F];
            body.extend_from_slice(chunk);
            out.extend([0xF0, 0x41, 0x10, MODEL_ID_800, 0x12]);
            out.extend(&body);
            out.push(checksum(&body));
            out.push(0xF7);
        }
        out
    }

    #[test]
    fn default_patch_wird_gueltiger_990_patch() {
        let p = convert(&DEFAULT_800);
        assert!(p.out_of_range().is_empty(), "{:?}", p.out_of_range());
        assert_eq!(p.get(find("patch.level").unwrap(), None), 0x64);
        assert_eq!(p.get(find("patch.structure_ab").unwrap(), None), 0);
        // LFO-Wellenform 800 → 990 (TRI, SAW, SQU, S&H, RND → 0, 2, 3, 5, 6)
        let t = |o: usize| DEFAULT_800[TONE_BASE_800 + o];
        assert_eq!(p.get(find("tone.lfo1.waveform").unwrap(), Some(Tone::A)), [0, 2, 3, 5, 6][t(5) as usize]);
        assert_eq!(p.get(find("tone.tvf.cutoff").unwrap(), Some(Tone::A)), t(37) as u16);
        assert_eq!(p.get(find("tone.wg.env_depth").unwrap(), Some(Tone::B)), 24);
    }

    #[test]
    fn bank_temp_und_intern_aus_sysex() {
        let mut named = DEFAULT_800;
        named[..16].copy_from_slice(b"JD800 Test Patch");
        let mut bytes = as_syx(&DEFAULT_800, BASE_TEMP);
        bytes.extend(as_syx(&named, BASE_INTERNAL + 3 * PATCH_SIZE_800 as u32));
        let found = patches_from_bytes(&bytes);
        assert_eq!(found.len(), 2);
        assert_eq!(found[1].1, "JD-800 I:14");
        assert_eq!(convert(&found[1].0).name(), "JD800 Test Patch");
        // JD-990-Dateien enthalten keine JD-800-Patches
        assert!(patches_from_bytes(&Patch::init().to_syx(0x10, crate::sysex::Address::TEMP_PATCH)).is_empty());
    }
}
