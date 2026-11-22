# ADR-27 — One write-permission rule, two checkpoints

| | |
|---|---|
| **Status** | proposed |
| **Date** | 2026-11-22 |
| **Origin** | Repo review 2026-11-22 |
| **Builds on** | ADR-7 (temp area is sacred), ADR-12 (logic lives in the Rust core), ADR-22 |
| **Change** | `openspec/changes/unify-write-guard` |

## Context

ADR-7 (addendum 2026-10-03) lists the only addresses the editor may write to. The rule
is implemented **twice, by hand**:

* Rust, `crates/jd990-midi/src/lib.rs:150` (`Device::allowed`), used by the CLI and the
  HIL tools.
* C++, `host/Source/DeviceLink.cpp:34` (`DeviceLink::allowedDt1`), used by the app,
  which talks MIDI through JUCE rather than `jd990-midi`.

The two have already drifted. For area `03` the C++ version requires block number 0
(`number == 0`), while the Rust version checks only the offset
(`0x03 => off + data.len() <= PATCH_SIZE`), so Rust accepts `03 05 00 00`. The address
is not documented as writable memory, but ADR-7 only permits `03 00 …`. A future
change to the whitelist (e.g. if writing to internal memory is ever enabled) would have
to be made twice and could silently differ again.

It is about to become three copies: `add-plugin-mode` task 3.1 moves the "Freigabeliste"
(whitelist) from `DeviceLink` into a new hub process.

ADR-12 already says a rule is implemented exactly once, in the Rust core.

## Decision

1. The write-permission rule lives **once**, in `jd990-core` (e.g. `sysex::write_allowed`
   on a parsed DT1: device, address, data).
2. There are still **two checkpoints** (defence in depth): `jd990-midi` calls the core
   rule before sending, and the host calls it through a new FFI function
   (`jd990_dt1_allowed(const uint8_t*, size_t)`) in `DeviceLink`.
3. A table of **shared test vectors** (allowed and refused messages, including the
   edge cases above) is checked in the Rust tests and in the C smoke test.
4. A refused write is never silent: in release builds `DeviceLink` logs it and shows it
   in the sync status, rather than only `jassertfalse`.

## Alternatives

* **Keep two implementations, add shared vectors only.** Catches drift that the
  vectors cover, but not new cases. Rejected because ADR-12 already prefers one rule.
* **Route all host MIDI through `jd990-midi`.** Too large: it would replace JUCE MIDI,
  port handling and the queue (ADR-8, ADR-10).

## Consequences

* The ADR-7 whitelist has one place to change and one test table.
* The host gains one FFI call per outgoing DT1. That's negligible against the MIDI rate
  limit (ADR-8).
