//! C-ABI zu `jd990-core` — die Brücke zum JUCE-Host (ADR-2, ADR-4).
//!
//! Header: `include/jd990.h` (von Hand gepflegt, Test in `tests/c_smoke.sh`).
//! Konventionen:
//! * Ein Patch ist ein undurchsichtiger Zeiger (`Jd990Patch*`), erzeugt mit
//!   `jd990_patch_*`, freigegeben mit `jd990_patch_free`.
//! * Parameter werden über einen stabilen Index 0 … `jd990_param_count()-1`
//!   angesprochen (Patch-Parameter zuerst, dann Tone-Parameter). `tone` ist
//!   0–3 für A–D, bei Patch-Parametern -1.
//! * Ausgabepuffer: Rückgabe = benötigte Länge; ist der Puffer zu klein, wird
//!   nichts geschrieben (aufrufen, Länge holen, erneut aufrufen).
//! * Fehler: negative Rückgabewerte (siehe `JD990_E_*` im Header).
//! * Kein MIDI hier — der Host sendet die gelieferten Bytes selbst.

use jd990_core::params::{PART_PARAMS, PATCH_PARAMS, PATCH_SIZE, PERF_PARAMS, PERF_SIZE, ParamDef, TONE_PARAMS};
use jd990_core::sysex::{Address, DEFAULT_DEVICE, Dt1, rq1};
use jd990_core::{Patch, Performance, Tone};
use std::ffi::c_char;

pub const E_NULL: i32 = -1;
pub const E_PARSE: i32 = -2;
pub const E_INDEX: i32 = -3;
pub const E_RANGE: i32 = -4;
pub const E_TONE: i32 = -5;

pub struct Jd990Patch(Patch);

/// Patch-Blöcke: 3 = Temp (0), 2 = Performance-Part (0–6 ≙ Part 1–7), 6 = Internal (0–63).
fn patch_base(area: u8, number: u8) -> Option<Address> {
    match (area, number) {
        (3, 0) => Some(Address::TEMP_PATCH),
        (2, 0..=6) => Some(Address::perf_patch(number)),
        (6, 0..=63) => Some(Address::internal_patch(number)),
        _ => None,
    }
}

fn param(index: u32) -> Option<&'static ParamDef> {
    let i = index as usize;
    PATCH_PARAMS.get(i).or_else(|| TONE_PARAMS.get(i.checked_sub(PATCH_PARAMS.len())?))
}

fn tone_of(def: &ParamDef, tone: i32) -> Result<Option<Tone>, i32> {
    let is_tone = def.id.starts_with("tone.");
    match (is_tone, tone) {
        (false, -1) => Ok(None),
        (true, 0..=3) => Ok(Some(Tone::ALL[tone as usize])),
        _ => Err(E_TONE),
    }
}

/// Kopiert `src` nach `out` (falls groß genug) und gibt die benötigte Länge zurück.
unsafe fn copy_out(src: &[u8], out: *mut u8, cap: usize) -> i32 {
    if !out.is_null() && cap >= src.len() {
        unsafe { std::ptr::copy_nonoverlapping(src.as_ptr(), out, src.len()) };
    }
    src.len() as i32
}

/// Wie `copy_out`, aber als nullterminierter C-String.
unsafe fn copy_str(s: &str, out: *mut c_char, cap: usize) -> i32 {
    let mut b = s.as_bytes().to_vec();
    b.push(0);
    unsafe { copy_out(&b, out as *mut u8, cap) }
}

#[unsafe(no_mangle)]
pub extern "C" fn jd990_spec_version() -> u32 {
    jd990_core::params::SPEC_VERSION
}

/// Leerer Patch (alle Bytes 0 — nicht gültig, nur als Startpunkt).
#[unsafe(no_mangle)]
pub extern "C" fn jd990_patch_new() -> *mut Jd990Patch {
    Box::into_raw(Box::new(Jd990Patch(Patch::from_bytes([0; PATCH_SIZE]))))
}

/// Init-Patch aus der Spec (gültig, trocken, nur Tone A aktiv).
#[unsafe(no_mangle)]
pub extern "C" fn jd990_patch_init() -> *mut Jd990Patch {
    Box::into_raw(Box::new(Jd990Patch(Patch::init())))
}

/// Kopie eines Patches (z. B. für Undo). NULL bei NULL.
///
/// # Safety
/// `p` gültig oder NULL.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_clone(p: *const Jd990Patch) -> *mut Jd990Patch {
    match unsafe { p.as_ref() } {
        Some(p) => Box::into_raw(Box::new(Jd990Patch(p.0.clone()))),
        None => std::ptr::null_mut(),
    }
}

/// 1, wenn beide Patches byteidentisch sind.
///
/// # Safety
/// `a`, `b` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_equal(a: *const Jd990Patch, b: *const Jd990Patch) -> i32 {
    match unsafe { (a.as_ref(), b.as_ref()) } {
        (Some(a), Some(b)) => (a.0 == b.0) as i32,
        _ => E_NULL,
    }
}

/// DT1-Nachrichten (aneinandergehängt) für den Temp-Bereich, die `from` auf dem
/// Gerät in `to` überführen — nur geänderte Parameter.
///
/// # Safety
/// `from`, `to` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_diff_dt1(from: *const Jd990Patch, to: *const Jd990Patch, out: *mut u8, cap: usize) -> i32 {
    match unsafe { (from.as_ref(), to.as_ref()) } {
        (Some(a), Some(b)) => {
            let bytes: Vec<u8> = a.0.diff_dt1(&b.0, DEFAULT_DEVICE, Address::TEMP_PATCH).iter().flat_map(Dt1::encode).collect();
            unsafe { copy_out(&bytes, out, cap) }
        }
        _ => E_NULL,
    }
}

/// Init-Wert eines Parameters (Doppelklick-Ziel), oder negativ.
#[unsafe(no_mangle)]
pub extern "C" fn jd990_param_init(index: u32) -> i32 {
    param(index).map_or(E_INDEX, |d| d.init as i32)
}

/// Patch aus .syx- oder .mid-Bytes (erster Patch; JD-800-Patches werden konvertiert). NULL bei Fehler.
///
/// # Safety
/// `data` muss auf `len` lesbare Bytes zeigen.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_from_bytes(data: *const u8, len: usize) -> *mut Jd990Patch {
    if data.is_null() {
        return std::ptr::null_mut();
    }
    let bytes = unsafe { std::slice::from_raw_parts(data, len) };
    match Patch::all_from_bytes(bytes) {
        Ok(v) if !v.is_empty() => Box::into_raw(Box::new(Jd990Patch(v[0].0.clone()))),
        // kein JD-990-Patch: JD-800-Patch? → konvertiert (M5)
        _ => match jd990_core::jd800::patches_from_bytes(bytes).first() {
            Some((p8, _)) => Box::into_raw(Box::new(Jd990Patch(jd990_core::jd800::convert(p8)))),
            None => std::ptr::null_mut(),
        },
    }
}

/// # Safety
/// `p` muss von `jd990_patch_*` stammen oder NULL sein; danach nicht mehr benutzen.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_free(p: *mut Jd990Patch) {
    if !p.is_null() {
        drop(unsafe { Box::from_raw(p) });
    }
}

/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_name(p: *const Jd990Patch, out: *mut c_char, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    unsafe { copy_str(&p.0.name(), out, cap) }
}

/// Patch als .syx in Gerätestückelung für den Temp-Bereich (`area` = 3) oder
/// Internal Patch n (`area` = 6, `number` 0–63).
///
/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_to_syx(p: *const Jd990Patch, area: u8, number: u8, out: *mut u8, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    let Some(base) = patch_base(area, number) else { return E_RANGE };
    unsafe { copy_out(&p.0.to_syx(DEFAULT_DEVICE, base), out, cap) }
}

#[unsafe(no_mangle)]
pub extern "C" fn jd990_param_count() -> u32 {
    (PATCH_PARAMS.len() + TONE_PARAMS.len()) as u32
}

/// Metadaten eines Parameters. `is_tone` = 1 bei Tone-Parametern.
#[repr(C)]
pub struct Jd990ParamInfo {
    pub min: u16,
    pub max: u16,
    pub offset: u16,
    pub bytes: u8,
    pub is_tone: u8,
    pub verified: u8,
    /// Reihenfolge der Gruppe im UI (group_order der Spec)
    pub group_rank: u16,
    /// Anzeigeart: 0 linear, 1 Enum, 2 Tabelle, 3 Pan, 4 Note, 5 Wellenform
    pub kind: u8,
}

/// # Safety
/// `info` zeigt auf beschreibbaren Speicher.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_param_info(index: u32, info: *mut Jd990ParamInfo) -> i32 {
    let Some(d) = param(index) else { return E_INDEX };
    let Some(info) = (unsafe { info.as_mut() }) else { return E_NULL };
    *info = info_of(d, d.id.starts_with("tone."));
    0
}

fn info_of(d: &ParamDef, per_unit: bool) -> Jd990ParamInfo {
    Jd990ParamInfo {
        min: d.min,
        max: d.max,
        offset: d.offset as u16,
        bytes: d.bytes as u8,
        is_tone: per_unit as u8,
        verified: d.verified as u8,
        group_rank: d.group_rank,
        kind: match d.display {
            jd990_core::params::Display::Linear { .. } => 0,
            jd990_core::params::Display::Enum => 1,
            jd990_core::params::Display::Table { .. } => 2,
            jd990_core::params::Display::Pan { .. } => 3,
            jd990_core::params::Display::Note => 4,
            jd990_core::params::Display::Wave => 5,
        },
    }
}

/// `what`: 0 = ID, 1 = Label, 2 = Gruppe.
///
/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_param_text(index: u32, what: u32, out: *mut c_char, cap: usize) -> i32 {
    let Some(d) = param(index) else { return E_INDEX };
    let s = match what {
        0 => d.id,
        1 => d.label,
        2 => d.group,
        _ => return E_RANGE,
    };
    unsafe { copy_str(s, out, cap) }
}

/// Index zu einer Parameter-ID, oder negativ.
///
/// # Safety
/// `id` ist ein gültiger C-String.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_param_find(id: *const c_char) -> i32 {
    if id.is_null() {
        return E_NULL;
    }
    let id = unsafe { std::ffi::CStr::from_ptr(id) }.to_string_lossy();
    (0..jd990_param_count()).find(|&i| param(i).unwrap().id == id).map_or(E_INDEX, |i| i as i32)
}

/// Rohwert, oder negativ bei Fehler.
///
/// # Safety
/// `p` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_get(p: *const Jd990Patch, index: u32, tone: i32) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    let Some(d) = param(index) else { return E_INDEX };
    match tone_of(d, tone) {
        Ok(t) => p.0.get(d, t) as i32,
        Err(e) => e,
    }
}

/// Setzt einen Rohwert (Bereich geprüft), wendet die Geräteregeln an und
/// liefert das DT1 für den **Temp-Bereich** zum Senden in `out`.
///
/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_set(p: *mut Jd990Patch, index: u32, tone: i32, value: u16, out: *mut u8, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_mut() }) else { return E_NULL };
    let Some(d) = param(index) else { return E_INDEX };
    let t = match tone_of(d, tone) {
        Ok(t) => t,
        Err(e) => return e,
    };
    if !d.in_range(value) {
        return E_RANGE;
    }
    // Erst prüfen, ob der Puffer reicht — sonst bliebe der Patch geändert, ohne dass
    // der Host die Nachricht bekommt.
    let mut trial = p.0.clone();
    let Ok((offset, bytes)) = trial.set(d, t, value) else { return E_RANGE };
    jd990_core::rules::apply_after_set(&mut trial, d, t);
    let msg = Dt1 { device: DEFAULT_DEVICE, address: Address::TEMP_PATCH.offset(offset), data: bytes }.encode();
    if out.is_null() || cap < msg.len() {
        return msg.len() as i32;
    }
    p.0 = trial;
    unsafe { copy_out(&msg, out, cap) }
}

/// Anzeige des aktuellen Werts im Kontext des Patches (z. B. „+12 st“).
///
/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_display(p: *const Jd990Patch, index: u32, tone: i32, out: *mut c_char, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    let Some(d) = param(index) else { return E_INDEX };
    match tone_of(d, tone) {
        Ok(t) => unsafe { copy_str(&jd990_core::display::format_in(&p.0, d, t), out, cap) },
        Err(e) => e,
    }
}

/// 1 = die UI soll diesen Rohwert sperren (out of scope, z. B. Kartenquellen).
#[unsafe(no_mangle)]
pub extern "C" fn jd990_param_locked(index: u32, raw: u16) -> i32 {
    param(index).map_or(E_INDEX, |d| d.locked.contains(&raw) as i32)
}

/// Anzeige eines beliebigen Rohwerts ohne Patch-Kontext (z. B. für Auswahllisten).
///
/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_param_format(index: u32, raw: u16, out: *mut c_char, cap: usize) -> i32 {
    let Some(d) = param(index) else { return E_INDEX };
    unsafe { copy_str(&jd990_core::display::format(d, raw), out, cap) }
}

/// RQ1 für den ganzen Temp Patch (Device 0x10).
///
/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_rq1_temp_patch(out: *mut u8, cap: usize) -> i32 {
    unsafe { copy_out(&rq1(DEFAULT_DEVICE, Address::TEMP_PATCH, PATCH_SIZE), out, cap) }
}

// ── Bibliothek (M5) ──────────────────────────────────────────────────────────
// Listen kommen als JSON (UTF-8), damit der Host keine C-Strukturen verwalten muss.

use jd990_library::{Error as LibError, Library};

pub struct Jd990Library(Library);

unsafe fn cstr<'a>(s: *const c_char) -> Option<std::borrow::Cow<'a, str>> {
    if s.is_null() { None } else { Some(unsafe { std::ffi::CStr::from_ptr(s) }.to_string_lossy()) }
}

/// Öffnet die Bibliothek (`path` NULL = Standardort bzw. JD990_LIBRARY). NULL bei Fehler.
///
/// # Safety
/// `path` ist NULL oder ein gültiger C-String.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_open(path: *const c_char) -> *mut Jd990Library {
    let p = unsafe { cstr(path) }.map(|s| std::path::PathBuf::from(s.as_ref())).unwrap_or_else(Library::default_path);
    match Library::open(p) {
        Ok(l) => Box::into_raw(Box::new(Jd990Library(l))),
        Err(_) => std::ptr::null_mut(),
    }
}

/// # Safety
/// `l` von `jd990_lib_open` oder NULL; danach nicht mehr benutzen.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_close(l: *mut Jd990Library) {
    if !l.is_null() {
        drop(unsafe { Box::from_raw(l) });
    }
}

/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_default_path(out: *mut c_char, cap: usize) -> i32 {
    unsafe { copy_str(&Library::default_path().to_string_lossy(), out, cap) }
}

/// Importiert eine Datei. Rückgabe 0, sonst negativ; Anzahl neu/Dubletten in `added`/`dupes`.
///
/// # Safety
/// `l` gültig, `path` gültiger C-String, `added`/`dupes` NULL oder beschreibbar.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_import_file(l: *const Jd990Library, path: *const c_char, added: *mut i32, dupes: *mut i32) -> i32 {
    let (Some(l), Some(path)) = (unsafe { l.as_ref() }, unsafe { cstr(path) }) else { return E_NULL };
    match l.0.import_file(path.as_ref()) {
        Ok(r) => {
            if let Some(a) = unsafe { added.as_mut() } { *a = r.added.len() as i32; }
            if let Some(d) = unsafe { dupes.as_mut() } { *d = r.duplicates.len() as i32; }
            0
        }
        Err(_) => E_PARSE,
    }
}

/// Einträge als JSON-Array `[{"id","name","source","added","notes","favorite","tags"}]`.
///
/// # Safety
/// `l` gültig, `query` gültiger C-String oder NULL, `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_list_json(l: *const Jd990Library, query: *const c_char, favorites_only: i32, out: *mut c_char, cap: usize) -> i32 {
    let Some(l) = (unsafe { l.as_ref() }) else { return E_NULL };
    let q = unsafe { cstr(query) }.unwrap_or_default();
    let Ok(list) = l.0.list(&q, favorites_only != 0) else { return E_PARSE };
    let json = serde_json::Value::Array(
        list.into_iter()
            .map(|e| serde_json::json!({ "id": e.id, "name": e.name, "source": e.source, "added": e.added,
                                         "notes": e.notes, "favorite": e.favorite, "tags": e.tags }))
            .collect(),
    );
    unsafe { copy_str(&json.to_string(), out, cap) }
}

/// Ähnlichste Einträge zu `p` als JSON `[[id, ähnlichkeit 0…1], …]`, absteigend.
///
/// # Safety
/// `l`, `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_similar_json(l: *const Jd990Library, p: *const Jd990Patch, limit: usize, out: *mut c_char, cap: usize) -> i32 {
    let (Some(l), Some(p)) = (unsafe { l.as_ref() }, unsafe { p.as_ref() }) else { return E_NULL };
    let Ok(list) = l.0.similar(&p.0, limit) else { return E_PARSE };
    let json = serde_json::Value::Array(list.into_iter().map(|(id, s)| serde_json::json!([id, s])).collect());
    unsafe { copy_str(&json.to_string(), out, cap) }
}

/// Patch eines Eintrags (neues Objekt, mit `jd990_patch_free` freigeben). NULL bei Fehler.
///
/// # Safety
/// `l` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_get(l: *const Jd990Library, id: i64) -> *mut Jd990Patch {
    match unsafe { l.as_ref() }.map(|l| l.0.get(id)) {
        Some(Ok(p)) => Box::into_raw(Box::new(Jd990Patch(p))),
        _ => std::ptr::null_mut(),
    }
}

/// Fügt einen Patch hinzu: > 0 neue ID, < 0 = −ID des vorhandenen gleichen Patches, 0 = Fehler.
///
/// # Safety
/// `l`, `p` gültig; `source` gültiger C-String oder NULL.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_add(l: *const Jd990Library, p: *const Jd990Patch, source: *const c_char) -> i64 {
    let (Some(l), Some(p)) = (unsafe { l.as_ref() }, unsafe { p.as_ref() }) else { return 0 };
    let src = unsafe { cstr(source) }.unwrap_or_default();
    match l.0.add(&p.0, &src) {
        Ok(id) => id,
        Err(LibError::Duplicate(id)) => -id,
        Err(_) => 0,
    }
}

/// # Safety
/// `l` gültig, `text` gültiger C-String.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_set_notes(l: *const Jd990Library, id: i64, text: *const c_char) -> i32 {
    let (Some(l), Some(t)) = (unsafe { l.as_ref() }, unsafe { cstr(text) }) else { return E_NULL };
    l.0.set_notes(id, &t).map_or(E_INDEX, |_| 0)
}

/// # Safety
/// `l` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_set_favorite(l: *const Jd990Library, id: i64, on: i32) -> i32 {
    let Some(l) = (unsafe { l.as_ref() }) else { return E_NULL };
    l.0.set_favorite(id, on != 0).map_or(E_INDEX, |_| 0)
}

/// Tags kommagetrennt, ersetzt die bisherigen.
///
/// # Safety
/// `l` gültig, `tags` gültiger C-String.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_set_tags(l: *const Jd990Library, id: i64, tags: *const c_char) -> i32 {
    let (Some(l), Some(t)) = (unsafe { l.as_ref() }, unsafe { cstr(tags) }) else { return E_NULL };
    l.0.set_tags(id, &t.split(',').collect::<Vec<_>>()).map_or(E_INDEX, |_| 0)
}

/// Entfernt einen Eintrag (nur auf ausdrückliche Nutzeraktion).
///
/// # Safety
/// `l` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_remove(l: *const Jd990Library, id: i64) -> i32 {
    let Some(l) = (unsafe { l.as_ref() }) else { return E_NULL };
    l.0.remove(id).map_or(E_INDEX, |_| 0)
}

/// Exportiert Einträge als .syx-Datei (1 → Temp-Patch, mehrere → Bank I:11 …).
///
/// # Safety
/// `l` gültig, `ids` zeigt auf `n` Werte, `path` gültiger C-String.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_lib_export(l: *const Jd990Library, ids: *const i64, n: usize, path: *const c_char) -> i32 {
    let (Some(l), Some(path)) = (unsafe { l.as_ref() }, unsafe { cstr(path) }) else { return E_NULL };
    if ids.is_null() || n == 0 { return E_NULL; }
    let ids = unsafe { std::slice::from_raw_parts(ids, n) };
    match l.0.export_syx(ids).map(|b| std::fs::write(path.as_ref(), b)) {
        Ok(Ok(())) => 0,
        _ => E_PARSE,
    }
}

/// Unterschiede zweier Patches als JSON `[["id","anzeige a","anzeige b"], …]`.
///
/// # Safety
/// `a`, `b` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_diff_json(a: *const Jd990Patch, b: *const Jd990Patch, out: *mut c_char, cap: usize) -> i32 {
    let (Some(a), Some(b)) = (unsafe { a.as_ref() }, unsafe { b.as_ref() }) else { return E_NULL };
    let d: Vec<_> = jd990_library::diff(&a.0, &b.0).into_iter().map(|(i, x, y)| serde_json::json!([i, x, y])).collect();
    unsafe { copy_str(&serde_json::Value::Array(d).to_string(), out, cap) }
}

/// Morph: neuer Patch zwischen `a` (t = 0) und `b` (t = 1). NULL bei NULL.
///
/// # Safety
/// `a`, `b` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_morph(a: *const Jd990Patch, b: *const Jd990Patch, t: f32) -> *mut Jd990Patch {
    match unsafe { (a.as_ref(), b.as_ref()) } {
        (Some(a), Some(b)) => Box::into_raw(Box::new(Jd990Patch(jd990_core::morph::morph(&a.0, &b.0, t)))),
        _ => std::ptr::null_mut(),
    }
}

/// Variation: gewichtete Zufallsänderung (amount 0…1, reproduzierbar über `seed`).
///
/// # Safety
/// `base` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_variation(base: *const Jd990Patch, amount: f32, seed: u64) -> *mut Jd990Patch {
    match unsafe { base.as_ref() } {
        Some(b) => Box::into_raw(Box::new(Jd990Patch(jd990_core::morph::variation(&b.0, amount, seed)))),
        None => std::ptr::null_mut(),
    }
}

/// RQ1 für einen ganzen Patch: `area` 3 = Temp (number 0), 6 = Internal (number 0–63).
///
/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_rq1_patch(area: u8, number: u8, out: *mut u8, cap: usize) -> i32 {
    let Some(at) = patch_base(area, number) else { return E_RANGE };
    unsafe { copy_out(&rq1(DEFAULT_DEVICE, at, PATCH_SIZE), out, cap) }
}

/// Tone kopieren: neuer Patch, in dem Tone `from` (0–3) nach `to` kopiert ist —
/// ganz (`group` NULL) oder nur eine Gruppe der Spec (z. B. "TVF ENV").
///
/// # Safety
/// `p` gültig; `group` NULL oder gültiger C-String.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_tone_copy(p: *const Jd990Patch, from: i32, to: i32, group: *const c_char) -> *mut Jd990Patch {
    let (Some(p), Ok(f), Ok(t)) = (unsafe { p.as_ref() }, usize::try_from(from), usize::try_from(to)) else { return std::ptr::null_mut() };
    let (Some(&f), Some(&t)) = (Tone::ALL.get(f), Tone::ALL.get(t)) else { return std::ptr::null_mut() };
    let g = unsafe { cstr(group) };
    let mut out = p.0.clone();
    out.copy_tone(f, t, g.as_deref());
    Box::into_raw(Box::new(Jd990Patch(out)))
}

/// Tones tauschen: neuer Patch mit vertauschten Tones `a` und `b` (0–3).
///
/// # Safety
/// `p` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_tone_swap(p: *const Jd990Patch, a: i32, b: i32) -> *mut Jd990Patch {
    let (Some(p), Ok(a), Ok(b)) = (unsafe { p.as_ref() }, usize::try_from(a), usize::try_from(b)) else { return std::ptr::null_mut() };
    let (Some(&a), Some(&b)) = (Tone::ALL.get(a), Tone::ALL.get(b)) else { return std::ptr::null_mut() };
    let mut out = p.0.clone();
    out.swap_tones(a, b);
    Box::into_raw(Box::new(Jd990Patch(out)))
}

// ── Performance-Modus (ADR-22) ──────────────────────────────────────────────

/// DT1-Nachrichten für den Temp-Patch (`03`) auf einen anderen Patch-Block umadressieren:
/// `area` 3 = Temp (number 0) oder 2 = Performance-Part (number 0–6). Der Offset im
/// Patch bleibt, die Prüfsumme wird neu berechnet. So gelten `jd990_patch_set`,
/// `_diff_dt1` und `_to_syx` unverändert für jeden Part. Nachrichten außerhalb von `03`
/// → Fehler (nichts wird stillschweigend woandershin geschrieben).
///
/// # Safety
/// `msgs` zeigt auf `len` Bytes; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_dt1_retarget(msgs: *const u8, len: usize, area: u8, number: u8, out: *mut u8, cap: usize) -> i32 {
    if msgs.is_null() {
        return E_NULL;
    }
    let base = match (area, number) {
        (3, 0) => Address::TEMP_PATCH,
        (2, 0..=6) => Address::perf_patch(number),
        _ => return E_RANGE,
    };
    let bytes = unsafe { std::slice::from_raw_parts(msgs, len) };
    let mut result = Vec::with_capacity(len);
    for m in jd990_core::sysex::split_messages(bytes) {
        let Ok(d) = Dt1::decode(m) else { return E_PARSE };
        if d.address.area() != 0x03 {
            return E_RANGE;
        }
        result.extend(Dt1 { device: d.device, address: base.offset(d.address.patch_offset()), data: d.data }.encode());
    }
    unsafe { copy_out(&result, out, cap) }
}

/// DT1 für den System-Modus (`00 00 00 00`): 0 = PATCH, 1 = PERFORMANCE.
/// RHYTHM (2) wird bewusst nicht erzeugt (ADR-22). Die einzige System-Adresse,
/// die der Editor schreiben darf.
///
/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_system_mode_dt1(mode: u8, out: *mut u8, cap: usize) -> i32 {
    if mode > 1 {
        return E_RANGE;
    }
    let msg = Dt1 { device: DEFAULT_DEVICE, address: Address::SYSTEM, data: vec![mode] }.encode();
    unsafe { copy_out(&msg, out, cap) }
}

/// RQ1 für den ganzen System-Bereich (38 Byte; Byte 0 = System-Modus).
///
/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_rq1_system(out: *mut u8, cap: usize) -> i32 {
    unsafe { copy_out(&rq1(DEFAULT_DEVICE, Address::SYSTEM, 38), out, cap) }
}

/// RQ1 für eine Performance: `area` 1 = Temp (number 0), 5 = Internal (0–15).
///
/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_rq1_performance(area: u8, number: u8, out: *mut u8, cap: usize) -> i32 {
    let at = match (area, number) {
        (1, 0) => Address::TEMP_PERFORMANCE,
        (5, 0..=15) => Address::internal_performance(number),
        _ => return E_RANGE,
    };
    unsafe { copy_out(&rq1(DEFAULT_DEVICE, at, PERF_SIZE), out, cap) }
}

pub struct Jd990Performance(Performance);

fn perf_param(index: u32) -> Option<&'static ParamDef> {
    let i = index as usize;
    PERF_PARAMS.get(i).or_else(|| PART_PARAMS.get(i.checked_sub(PERF_PARAMS.len())?))
}

/// Part-Parameter brauchen `part` 0–7, Common-Parameter −1.
fn part_of(def: &ParamDef, part: i32) -> Result<Option<usize>, i32> {
    match (def.id.starts_with("part."), part) {
        (false, -1) => Ok(None),
        (true, 0..=7) => Ok(Some(part as usize)),
        _ => Err(E_TONE),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn jd990_perf_param_count() -> u32 {
    (PERF_PARAMS.len() + PART_PARAMS.len()) as u32
}

/// Wie `jd990_param_info`; `is_tone` = 1 heißt hier „je Part“.
///
/// # Safety
/// `info` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_param_info(index: u32, info: *mut Jd990ParamInfo) -> i32 {
    let Some(d) = perf_param(index) else { return E_INDEX };
    let Some(info) = (unsafe { info.as_mut() }) else { return E_NULL };
    *info = info_of(d, d.id.starts_with("part."));
    0
}

/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_param_text(index: u32, what: u32, out: *mut c_char, cap: usize) -> i32 {
    let Some(d) = perf_param(index) else { return E_INDEX };
    let s = match what {
        0 => d.id,
        1 => d.label,
        2 => d.group,
        _ => return E_RANGE,
    };
    unsafe { copy_str(s, out, cap) }
}

#[unsafe(no_mangle)]
pub extern "C" fn jd990_perf_param_init(index: u32) -> i32 {
    perf_param(index).map_or(E_INDEX, |d| d.init as i32)
}

/// # Safety
/// `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_param_format(index: u32, raw: u16, out: *mut c_char, cap: usize) -> i32 {
    let Some(d) = perf_param(index) else { return E_INDEX };
    unsafe { copy_str(&jd990_core::display::format(d, raw), out, cap) }
}

#[unsafe(no_mangle)]
pub extern "C" fn jd990_perf_init() -> *mut Jd990Performance {
    Box::into_raw(Box::new(Jd990Performance(Performance::init())))
}

/// # Safety
/// `p` gültig oder NULL.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_clone(p: *const Jd990Performance) -> *mut Jd990Performance {
    match unsafe { p.as_ref() } {
        Some(p) => Box::into_raw(Box::new(Jd990Performance(p.0.clone()))),
        None => std::ptr::null_mut(),
    }
}

/// # Safety
/// `p` von `jd990_perf_*` oder NULL; danach nicht mehr benutzen.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_free(p: *mut Jd990Performance) {
    if !p.is_null() {
        drop(unsafe { Box::from_raw(p) });
    }
}

/// Erste Performance (Temp `01` bevorzugt, sonst Internal/Card) aus .syx/.mid-Bytes. NULL, wenn keine.
///
/// # Safety
/// `data` zeigt auf `len` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_from_bytes(data: *const u8, len: usize) -> *mut Jd990Performance {
    if data.is_null() {
        return std::ptr::null_mut();
    }
    let bytes = unsafe { std::slice::from_raw_parts(data, len) };
    let Ok(all) = Performance::all_from_bytes(bytes) else { return std::ptr::null_mut() };
    let pick = all.iter().find(|(_, a)| *a == Address::TEMP_PERFORMANCE).or(all.first());
    pick.map_or(std::ptr::null_mut(), |(p, _)| Box::into_raw(Box::new(Jd990Performance(p.clone()))))
}

/// Patch eines bestimmten Blocks aus .syx-Bytes (z. B. Part 3 eines Setups: area 2, number 2). NULL, wenn nicht enthalten.
///
/// # Safety
/// `data` zeigt auf `len` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_patch_from_bytes_at(data: *const u8, len: usize, area: u8, number: u8) -> *mut Jd990Patch {
    let (false, Some(base)) = (data.is_null(), patch_base(area, number)) else { return std::ptr::null_mut() };
    let bytes = unsafe { std::slice::from_raw_parts(data, len) };
    match Patch::all_from_bytes(bytes) {
        Ok(v) => v.into_iter().find(|(_, a)| *a == base).map_or(std::ptr::null_mut(), |(p, _)| Box::into_raw(Box::new(Jd990Patch(p)))),
        Err(_) => std::ptr::null_mut(),
    }
}

/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_name(p: *const Jd990Performance, out: *mut c_char, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    unsafe { copy_str(&p.0.name(), out, cap) }
}

/// Neuer Name; liefert das DT1 (Temp-Performance) in `out`.
///
/// # Safety
/// `p` gültig, `name` C-String; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_set_name(p: *mut Jd990Performance, name: *const c_char, out: *mut u8, cap: usize) -> i32 {
    let (Some(p), Some(name)) = (unsafe { p.as_mut() }, unsafe { cstr(name) }) else { return E_NULL };
    let mut trial = p.0.clone();
    let (off, data) = trial.set_name(&name);
    let msg = Dt1 { device: DEFAULT_DEVICE, address: Address::TEMP_PERFORMANCE.offset(off), data }.encode();
    if out.is_null() || cap < msg.len() {
        return msg.len() as i32;
    }
    p.0 = trial;
    unsafe { copy_out(&msg, out, cap) }
}

/// # Safety
/// `p` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_get(p: *const Jd990Performance, index: u32, part: i32) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    let Some(d) = perf_param(index) else { return E_INDEX };
    match part_of(d, part) {
        Ok(pt) => p.0.get(d, pt) as i32,
        Err(e) => e,
    }
}

/// Setzt einen Rohwert (Bereich + Stimmenregel) und liefert das DT1 (Temp-Performance) in `out`.
///
/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_set(p: *mut Jd990Performance, index: u32, part: i32, value: u16, out: *mut u8, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_mut() }) else { return E_NULL };
    let Some(d) = perf_param(index) else { return E_INDEX };
    let pt = match part_of(d, part) {
        Ok(pt) => pt,
        Err(e) => return e,
    };
    let mut trial = p.0.clone();
    let Ok((offset, data)) = trial.set(d, pt, value) else { return E_RANGE };
    let msg = Dt1 { device: DEFAULT_DEVICE, address: Address::TEMP_PERFORMANCE.offset(offset), data }.encode();
    if out.is_null() || cap < msg.len() {
        return msg.len() as i32;
    }
    p.0 = trial;
    unsafe { copy_out(&msg, out, cap) }
}

/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_display(p: *const Jd990Performance, index: u32, part: i32, out: *mut c_char, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    let Some(d) = perf_param(index) else { return E_INDEX };
    match part_of(d, part) {
        Ok(pt) => unsafe { copy_str(&p.0.display(d, pt), out, cap) },
        Err(e) => e,
    }
}

/// Reservierte Stimmen (Summe Part 1–8, höchstens 24).
///
/// # Safety
/// `p` gültig.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_voices(p: *const Jd990Performance) -> i32 {
    unsafe { p.as_ref() }.map_or(E_NULL, |p| p.0.voices_reserved() as i32)
}

/// Patch-Platz eines Parts als Text („I:11“, „C:25“, „A:11“ …).
///
/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_patch_slot(p: *const Jd990Performance, part: i32, out: *mut c_char, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    if !(0..8).contains(&part) {
        return E_TONE;
    }
    unsafe { copy_str(&p.0.patch_slot(part as usize), out, cap) }
}

/// Performance als .syx: `area` 1 = Temp (number 0), 5 = Internal (0–15, nur Export).
///
/// # Safety
/// `p` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_to_syx(p: *const Jd990Performance, area: u8, number: u8, out: *mut u8, cap: usize) -> i32 {
    let Some(p) = (unsafe { p.as_ref() }) else { return E_NULL };
    let base = match (area, number) {
        (1, 0) => Address::TEMP_PERFORMANCE,
        (5, 0..=15) => Address::internal_performance(number),
        _ => return E_RANGE,
    };
    unsafe { copy_out(&p.0.to_syx(DEFAULT_DEVICE, base), out, cap) }
}

/// DT1 (Temp-Performance), die `from` auf dem Gerät in `to` überführen.
///
/// # Safety
/// `from`, `to` gültig; `out` hat Platz für `cap` Bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jd990_perf_diff_dt1(from: *const Jd990Performance, to: *const Jd990Performance, out: *mut u8, cap: usize) -> i32 {
    let (Some(a), Some(b)) = (unsafe { from.as_ref() }, unsafe { to.as_ref() }) else { return E_NULL };
    let bytes: Vec<u8> = a.0.diff_dt1(&b.0, DEFAULT_DEVICE, Address::TEMP_PERFORMANCE).iter().flat_map(Dt1::encode).collect();
    unsafe { copy_out(&bytes, out, cap) }
}

