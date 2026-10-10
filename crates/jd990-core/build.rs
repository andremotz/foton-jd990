//! Codegen aus `spec/params.toml` (ADR-3): Parametertabellen als Rust-Konstanten.
//! Die Spec wird hier validiert — eine ungültige Spec bricht den Build ab.

use jd990_spec::{Param, Spec};
use std::fmt::Write;

fn main() {
    let path = jd990_spec::default_spec_path();
    let wpath = jd990_spec::default_waveforms_path();
    println!("cargo:rerun-if-changed={}", path.display());
    println!("cargo:rerun-if-changed={}", wpath.display());
    let waves = jd990_spec::load_waveforms(&wpath).unwrap_or_else(|e| panic!("{e}"));
    let spec = Spec::load(&path).unwrap_or_else(|e| panic!("Spec nicht lesbar: {e}"));
    let errors = spec.validate();
    assert!(errors.is_empty(), "Spec ungültig:\n{}", errors.join("\n"));

    let mut out = String::new();
    writeln!(out, "// GENERIERT aus spec/params.toml durch build.rs — nicht von Hand ändern.").unwrap();
    writeln!(out, "pub const MODEL_ID: u8 = {:#04x};", spec.meta.model_id).unwrap();
    writeln!(out, "pub const SPEC_VERSION: u32 = {};", spec.meta.spec_version).unwrap();
    writeln!(out, "pub const PATCH_SIZE: usize = {};", spec.patch.size).unwrap();
    writeln!(out, "pub const COMMON_SIZE: usize = {};", spec.patch.common_size).unwrap();
    writeln!(out, "pub const TONE_SIZE: usize = {};", spec.tone.size).unwrap();
    writeln!(out, "pub const NAME_OFFSET: usize = {};", spec.patch.name_offset).unwrap();
    writeln!(out, "pub const NAME_LENGTH: usize = {};", spec.patch.name_length).unwrap();
    writeln!(out, "pub const TONE_BASES: [usize; 4] = {:?};", spec.meta.tone_bases.all()).unwrap();
    writeln!(out, "/// Index 0 = \"None\"; Name der Wellenform mit Rohwert r = WAVEFORM_NAMES[r + 1]").unwrap();
    writeln!(out, "pub static WAVEFORM_NAMES: [&str; {}] = {:?};", waves.names.len(), waves.names).unwrap();
    // Init-Patch: Regel + Ausnahmen aus [init], zur Build-Zeit berechnet
    let mut init = vec![0u8; spec.patch.size];
    let name = format!("{:<16}", spec.init.name);
    init[spec.patch.name_offset..spec.patch.name_offset + 16].copy_from_slice(name.as_bytes());
    let mut put = |off: usize, p: &jd990_spec::Param, v: u16| {
        if p.bytes == 2 {
            init[off] = (v >> 7) as u8;
            init[off + 1] = (v & 0x7F) as u8;
        } else {
            init[off] = v as u8;
        }
    };
    for p in &spec.patch.param {
        put(p.offset, p, spec.init_value(p));
    }
    for base in spec.meta.tone_bases.all() {
        for p in &spec.tone.param {
            put(base + p.offset, p, spec.init_value(p));
        }
    }
    writeln!(out, "/// Init-Patch (Regel + `[init]` der Spec), zur Build-Zeit berechnet").unwrap();
    writeln!(out, "pub static INIT_PATCH: [u8; {}] = {:?};", spec.patch.size, init).unwrap();
    let pf = &spec.performance;
    writeln!(out, "pub const PERF_SIZE: usize = {};", pf.size).unwrap();
    writeln!(out, "pub const PERF_NAME_OFFSET: usize = {};", pf.name_offset).unwrap();
    writeln!(out, "pub const PERF_NAME_LENGTH: usize = {};", pf.name_length).unwrap();
    writeln!(out, "pub const PART_BASE: usize = {};", pf.part_base).unwrap();
    writeln!(out, "pub const PART_SIZE: usize = {};", pf.part_size).unwrap();
    writeln!(out, "pub const PART_COUNT: usize = {};", pf.part_count).unwrap();
    emit(&mut out, "PERF_PARAMS", &pf.param, &pf.group_order, &spec);
    emit(&mut out, "PART_PARAMS", &pf.part_param, &pf.group_order, &spec);
    emit(&mut out, "PATCH_PARAMS", &spec.patch.param, &spec.patch.group_order, &spec);
    emit(&mut out, "TONE_PARAMS", &spec.tone.param, &spec.tone.group_order, &spec);

    let dest = std::path::Path::new(&std::env::var("OUT_DIR").unwrap()).join("params.rs");
    std::fs::write(dest, out).unwrap();
}

fn emit(out: &mut String, name: &str, params: &[Param], order: &[String], spec: &Spec) {
    writeln!(out, "pub static {name}: &[ParamDef] = &[").unwrap();
    for p in params {
        let values = match p.display.kind.as_str() {
            "enum" => p.display.table.as_ref().map(|t| spec.enums[t].values.clone()),
            "table" => p.display.table.as_ref().and_then(|t| spec.tables[t].values.clone()),
            _ => None,
        };
        let values = values.map_or("None".to_string(), |v| format!("Some(&{v:?})"));
        let d = &p.display;
        let unit = d.unit.clone().unwrap_or_default();
        let display = match d.kind.as_str() {
            "linear" => {
                let labels: Vec<String> = d.labels.iter().map(|(k, v)| format!("({k}, {v:?})")).collect();
                format!(
                    "Display::Linear {{ min: {:?}, max: {:?}, unit: {unit:?}, extra: &{:?}, labels: &[{}] }}",
                    d.min.unwrap(), d.max.unwrap(), d.extra, labels.join(", ")
                )
            }
            "enum" => "Display::Enum".into(),
            "table" => {
                let t = &spec.tables[d.table.as_ref().unwrap()];
                let segs: Vec<String> = t.segments.iter().flatten().map(|s| format!("({}, {:?}, {:?})", s.from, s.start, s.step)).collect();
                let (nf, nv) = t.notes.as_ref().map_or((u16::MAX, vec![]), |n| (n.from, n.values.clone()));
                format!(
                    "Display::Table {{ unit: {:?}, segments: &[{}], notes_from: {nf}, notes: &{nv:?} }}",
                    t.unit.clone().unwrap_or_default(), segs.join(", ")
                )
            }
            "pan" => format!("Display::Pan {{ extra: &{:?} }}", d.extra),
            "note" => "Display::Note".into(),
            "wavelist" => "Display::Wave".into(),
            k => panic!("unbekannte Anzeige {k}"),
        };
        let locked = match d.kind.as_str() {
            "enum" => spec.enums[d.table.as_ref().unwrap()].out_of_scope.clone(),
            _ => vec![],
        };
        let rank = order.iter().position(|g| *g == p.group).unwrap_or_else(|| panic!("{}: Gruppe {} fehlt in group_order", p.id, p.group));
        writeln!(
            out,
            "    ParamDef {{ id: {:?}, label: {:?}, group: {:?}, offset: {}, bytes: {}, min: {}, max: {}, verified: {}, values: {}, display: {}, group_rank: {}, locked: &{:?}, init: {} }},",
            p.id, p.label, p.group, p.offset, p.bytes, p.raw.min, p.raw.max, p.verified, values, display, rank, locked, spec.init_value(p)
        )
        .unwrap();
    }
    writeln!(out, "];").unwrap();
}
