# Design

## Context

- The Rust code deliberately uses long lines (282 lines over 100 columns in `crates/*/src`).
  A default rustfmt run would touch most files.
- `add-release-builds` will add a Windows job and a release workflow to the same files.
  This change should land first, or rebase cleanly, so both don't edit `ci.yml` at once.
- ADR-15: 0-pixel tolerance, deterministic on the *same* machine; font rendering may differ
  on another macOS version. Hosted `macos-latest` changes its image over time.

## Goals / Non-Goals

**Goals:**
- A pull request with unformatted code, a vulnerable or incompatibly licensed dependency,
  or an FFI signature drift fails CI.
- CI runs get faster through caching.

**Non-Goals:**
- Changing the local 0-px workflow of ADR-15.
- Enforcing formatting of the C++ host (could follow with `clang-format` later).

## Decisions

1. **`max_width = 140`** in `rustfmt.toml`. It keeps the current style and halves the
   formatting diff (137 vs. 281 hunks). The reformat goes in its own commit, listed in
   `.git-blame-ignore-revs`.
2. **`cargo deny` licence allow-list**, starting from what `cargo metadata` reports
   today: MIT, Apache-2.0 (incl. `WITH LLVM-exception`), Unlicense, BSD-2-Clause, ISC,
   Zlib, Unicode-3.0, AGPL-3.0-or-later (own crates). Anything new needs a decision.
3. **Header check via `cbindgen`, comparison not replacement.** The hand-written header has
   German comments, error-code macros and grouping that cbindgen wouldn't reproduce. CI generates a
   header into `target/` and a small script compares the function signatures.
4. **UI images on CI: spike first.** Run `host/tests/ui_snapshots.sh` on `macos-latest`
   three times over a week. If it's stable at 0 px, make it a required step. If not, run it
   with `UI_TOLERANCE` as a non-blocking step that uploads diffs, and correct ADR-15.

## Risks / Trade-offs

- [Formatting commit conflicts with open branches] → merge it when no change branch is open,
  or rebase branches with `cargo fmt` afterwards.
- [Dependabot noise] → weekly, grouped minor/patch updates.
