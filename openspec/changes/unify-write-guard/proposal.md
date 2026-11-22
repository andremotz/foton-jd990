# Proposal

## Why

The ADR-7 write whitelist is implemented twice by hand: `Device::allowed` in
`crates/jd990-midi/src/lib.rs:150` and `DeviceLink::allowedDt1` in
`host/Source/DeviceLink.cpp:34`. The two already differ: for area `03` the Rust version
does not check the block number, so it accepts `03 05 00 00`, which the host rejects and
ADR-7 does not permit. `add-plugin-mode` (task 3.1) would add a third copy in the hub.
Protecting the device's 1993 battery-backed memory is the project's most important
promise, so the rule should exist exactly once (ADR-27).

## What Changes

- New core function `jd990_core::sysex::write_allowed(&Dt1) -> bool` (or on raw bytes),
  the only implementation of the ADR-7 table.
- `jd990-midi` `Device::allowed` delegates to it; the Rust `03` block-number gap is closed.
- New FFI function `jd990_dt1_allowed(const uint8_t* msg, size_t len)` in
  `jd990-ffi` + `include/jd990.h`; `DeviceLink::allowedDt1` calls it.
- Shared test vectors (allowed and refused, with edge cases) in one file, used by
  Rust tests and `crates/jd990-ffi/tests/smoke.c`.
- A refused write is logged and shown in the sync status in release builds, not
  only `jassertfalse`.

**Not included:** any change to *what* is writable (ADR-7 stays as is); routing host
MIDI through `jd990-midi`; enabling writes to internal memory; the plugin hub itself.

**Branch:** `fix/unify-write-guard`. Merge into `main` only after the acceptance group
(task group 4).

## Capabilities

### New Capabilities
- `device-write-guard`: Which SysEx writes may leave the editor, and where that is enforced.

### Modified Capabilities
- (none)

## Impact

- `crates/jd990-core/src/sysex.rs`, `crates/jd990-midi/src/lib.rs`
- `crates/jd990-ffi/src/lib.rs`, `crates/jd990-ffi/include/jd990.h`, `crates/jd990-ffi/tests/smoke.c`
- `host/Source/DeviceLink.cpp/.h`
- ADR-7 (addendum: where the rule lives), ADR-27 (accepted on merge)
