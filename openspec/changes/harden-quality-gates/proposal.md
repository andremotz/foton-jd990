# Proposal

## Why

CI (`.github/workflows/ci.yml`) runs clippy, tests and the C smoke test on Ubuntu and
builds the macOS app. Several cheap checks are missing, and one documented check isn't
actually there:

- **Formatting is not enforced.** `cargo fmt --all --check` reports 281 diff hunks with
  default settings (137 at `max_width = 140`); there is no `rustfmt.toml`.
- **UI reference images no longer run in CI.** The GitLab CI ran them in its
  `host-macos` job (ADR-20). The GitHub CI that replaced it (ADR-23) only builds the
  app, so the check was lost in the move and ADR-15's CI remark is stale.
- **The C header is maintained by hand** (`crates/jd990-ffi/include/jd990.h`). The smoke
  test compiles against it but can't notice a signature that differs from the Rust side.
- **No dependency hygiene:** no Dependabot, no `cargo deny` (advisories, and licences
  compatible with AGPL-3.0, which matters for a published AGPL project).
- **No caching:** every macOS run re-clones JUCE and rebuilds the Rust core from scratch.

## What Changes

- `rustfmt.toml` + one formatting-only commit, then `cargo fmt --all --check` in CI.
- `deny.toml` + `cargo deny check` (advisories, licences, bans, sources) in the Rust job.
- `.github/dependabot.yml` for `cargo` and `github-actions` (weekly, grouped).
- `Swatinem/rust-cache` in both jobs; JUCE checkout cached by version.
- FFI header drift check: generate a reference header with `cbindgen` in CI and compare
  declarations with `include/jd990.h` (the hand-written header stays the source, with comments).
- UI reference images: decide between running them on the hosted macOS runner (with a
  tolerance and the diff images uploaded as an artifact) and correcting ADR-15. The
  spike result decides; see design.md.
- `rust-version` (MSRV) in the workspace `Cargo.toml`, checked by CI.

**Not included:** Windows CI and release builds (change `add-release-builds`), code
signing, coverage reporting, rewriting tests, any functional code change.

**Branch:** `ci/quality-gates`. Merge into `main` only after the acceptance group
(task group 5).

## Capabilities

### New Capabilities
- `ci-quality-gates`: Which automated checks every push and pull request must pass.

### Modified Capabilities
- (none)

## Impact

- `.github/workflows/ci.yml`, new `.github/dependabot.yml`, `deny.toml`, `rustfmt.toml`
- All Rust sources (formatting only, one separate commit)
- `Cargo.toml` (`rust-version`), ADR-15 (addendum), README "Tests"
