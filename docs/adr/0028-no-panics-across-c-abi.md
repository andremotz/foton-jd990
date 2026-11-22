# ADR-28 — No panic may cross the C ABI

| | |
|---|---|
| **Status** | proposed |
| **Date** | 2026-11-22 |
| **Origin** | Repo review 2026-11-22, with an eye on `add-plugin-mode` |
| **Builds on** | ADR-2 (Rust core with C ABI) |

## Context

`crates/jd990-ffi/src/lib.rs` exports about 65 `extern "C"` functions and has no
`catch_unwind`. When Rust code panics inside an `extern "C"` function, the process
aborts. Today that only kills the standalone app. In plugin mode (`add-plugin-mode`)
it would take down the **DAW and the user's unsaved project**.

What the review found:

* Host-supplied inputs are checked before use: null pointers, `index` via
  `param()`/`perf_param()`, `tone`/`part` via `tone_of`/`part_of`, part range in
  `jd990_perf_patch_slot` (`lib.rs:870`).
* The remaining `unwrap()` calls outside tests are lookups of static spec entries, e.g.
  `params::find_perf("part.bank").unwrap()` (`crates/jd990-core/src/performance.rs:57–60,
  139–154`) and `param(i).unwrap()` (`crates/jd990-ffi/src/lib.rs:279`). They can only
  fail if `spec/*.toml` changes. They're invariants, not input errors.

The byte-buffer entry points (`jd990_patch_from_bytes`, `jd990_perf_from_bytes` and
others, `lib.rs:163, 622, 765, 778`) pass user-chosen `.syx`/`.mid`/JD-800 files to the
SysEx, SMF and JD-800 parsers. Those parsers were not read line by line in the review.
A quick check fed eight malformed files (truncated, random bytes, broken SMF header,
invalid part number) to `jd990 show` and `jd990 convert`. None of them panicked
(exit code 1 or 0, never 101). That is a sample, not a proof.

So no panic path from host input is known today. The risk is a future one: a spec edit,
a new FFI function or a parser input nobody tried yet.

## Decision

1. Every exported function body runs inside a small guard (`ffi_guard(default, || …)`
   built on `std::panic::catch_unwind`). A caught panic returns a new error code
   `JD990_E_INTERNAL` (or NULL) and records the message, which the host can fetch with
   `jd990_last_error`.
2. Static spec lookups become `expect("spec: part.bank")`, or better, constants
   resolved once at build time by the codegen (`crates/jd990-core/build.rs`), so a
   broken spec fails `cargo test` and not a user session.
3. Parsers of external bytes get a fuzz/property test ("never panics on arbitrary
   input", backlog T5).
4. This becomes a **precondition for the plugin build**. A task for it is to be added
   to `add-plugin-mode` (backlog B1).

## Consequences

* A bug in the core shows up as an error message in the editor rather than a crashed DAW.
* A little boilerplate per FFI function. A macro keeps it to one line.
