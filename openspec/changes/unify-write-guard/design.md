# Design

## Context

- ADR-7 addendum 2026-10-03 is the authoritative table: `03 00 …` (temp patch, 486 bytes),
  `01 00 …` (temp performance, 144 bytes), `02 00 …`–`02 06 …` (part patches),
  `00 00 00 00` with exactly one byte 0 or 1 (system mode). Everything else refused.
- The CLI and HIL tools write through `jd990-midi`. The app writes through JUCE MIDI in
  `DeviceLink` and gets its bytes from the FFI (ADR-12).
- The C++ check also validates the message frame (`F0 41 dev 57 12 … F7`, min. length).
  The Rust check takes an already parsed address and data.

## Goals / Non-Goals

**Goals:**
- One implementation of the table; both send paths call it.
- Drift between table, Rust and host is caught by tests without hardware.

**Non-Goals:**
- Checksum validation of outgoing messages (the core builds them).
- Read requests (RQ1); they can't change device memory.

## Decisions

1. **Rule on raw bytes in the core.** `write_allowed_bytes(&[u8]) -> bool` parses the
   frame and delegates to `write_allowed(address, data)`. The host can pass exactly what
   it is about to send, so the check covers the real bytes.
2. **Device ID ignored by the rule.** Any unit # (0x10–0x1F) is fine; the rule is about
   addresses.
3. **Test vectors as a Rust table + generated C array.** `build.rs` or a small test
   writes `tests/write_vectors.h`; the C smoke test includes it. Alternative: a TOML file
   in `spec/`. Rejected for now, it isn't parameter spec.
4. **Refusal visible.** `DeviceLink` counts refused messages; the sync status shows
   "write refused (address …)". Release builds no longer fail silently.

## Risks / Trade-offs

- [Behaviour change in Rust: `03 nn` with nn ≠ 0 is now refused] → nothing in the code
  writes there (`Address::TEMP_PATCH` is `03 00 00 00`). Covered by a test vector.
- [Extra FFI call per DT1] → negligible against the ADR-8 rate limit.
