//! Shared write-permission test vectors (ADR-27) as a C header for the smoke test.
//!
//! The header `tests/write_vectors.h` is generated from
//! `jd990_core::sysex::write_vectors()` and checked in. This test fails when it is out
//! of date; regenerate with:
//!
//! ```sh
//! UPDATE_WRITE_VECTORS=1 cargo test -p jd990-ffi --test write_vectors
//! ```

use std::fmt::Write;
use std::path::Path;

fn header() -> String {
    let vectors = jd990_core::sysex::write_vectors();
    let mut h = String::new();
    h.push_str(
        "/* Generated from jd990_core::sysex::write_vectors() by crates/jd990-ffi/tests/write_vectors.rs.\n \
         * Do not edit. Regenerate: UPDATE_WRITE_VECTORS=1 cargo test -p jd990-ffi --test write_vectors */\n\
         #ifndef JD990_WRITE_VECTORS_H\n#define JD990_WRITE_VECTORS_H\n\n\
         #include <stddef.h>\n#include <stdint.h>\n\n\
         typedef struct {\n    const char *name;\n    int allowed;\n    size_t len;\n    const uint8_t *msg;\n} Jd990WriteVector;\n\n",
    );
    for (i, v) in vectors.iter().enumerate() {
        writeln!(h, "static const uint8_t jd990_wv_{i}[] = {{ /* {} */", v.name).unwrap();
        for chunk in v.msg.chunks(16) {
            let bytes: Vec<String> = chunk.iter().map(|b| format!("0x{b:02X}")).collect();
            writeln!(h, "    {},", bytes.join(", ")).unwrap();
        }
        h.push_str("};\n");
    }
    h.push_str("\nstatic const Jd990WriteVector jd990_write_vectors[] = {\n");
    for (i, v) in vectors.iter().enumerate() {
        writeln!(h, "    {{ \"{}\", {}, sizeof jd990_wv_{i}, jd990_wv_{i} }},", v.name, v.allowed as u8).unwrap();
    }
    h.push_str(
        "};\n\n#define JD990_WRITE_VECTOR_COUNT (sizeof jd990_write_vectors / sizeof jd990_write_vectors[0])\n\n\
         #endif\n",
    );
    h
}

#[test]
fn c_header_matches_core_vectors() {
    let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/write_vectors.h");
    let want = header();
    if std::env::var_os("UPDATE_WRITE_VECTORS").is_some() {
        std::fs::write(&path, &want).unwrap();
        return;
    }
    // CRLF-tolerant: a Windows checkout with autocrlf must not fail the comparison
    let have = std::fs::read_to_string(&path).unwrap_or_default().replace("\r\n", "\n");
    assert!(
        have == want,
        "{} is out of date: UPDATE_WRITE_VECTORS=1 cargo test -p jd990-ffi --test write_vectors",
        path.display()
    );
}

#[test]
fn ffi_follows_core_vectors() {
    assert_eq!(unsafe { jd990_ffi::jd990_dt1_allowed(std::ptr::null(), 0) }, jd990_ffi::E_NULL);
    for v in jd990_core::sysex::write_vectors() {
        let got = unsafe { jd990_ffi::jd990_dt1_allowed(v.msg.as_ptr(), v.msg.len()) };
        assert_eq!(got, v.allowed as i32, "{}", v.name);
    }
}
