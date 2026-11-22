# Tasks

Branch: `ci/quality-gates`

## 1. Formatting

- [ ] 1.1 `rustfmt.toml` (`max_width = 140`, edition 2024)
- [ ] 1.2 Formatting-only commit (`cargo fmt --all`), hash added to `.git-blame-ignore-revs`
- [ ] 1.3 `cargo fmt --all --check` step in the Rust job

## 2. Dependencies

- [ ] 2.1 `deny.toml` (licence allow-list per design.md, advisories, sources) and `cargo deny check` in CI
- [ ] 2.2 `.github/dependabot.yml` for `cargo` and `github-actions`, weekly, grouped
- [ ] 2.3 `rust-version` in the workspace manifest; CI job on that toolchain (`cargo check`)

## 3. Speed and FFI consistency

- [ ] 3.1 `Swatinem/rust-cache` in both jobs; JUCE checkout cached by version key
- [ ] 3.2 `cbindgen` reference header in CI + compare script; fails on drift
- [ ] 3.3 Fix any drift the check finds (separate commit)

## 4. UI reference images

- [ ] 4.1 Spike: `ui_snapshots.sh` on `macos-latest`, three runs on different days; record pixel differences
- [ ] 4.2 Required step (stable) or non-blocking step with `UI_TOLERANCE` (unstable); diff images uploaded as artifact
- [ ] 4.3 ADR-15 addendum with the result; README "Tests" updated

## 5. Acceptance before merge

- [ ] 5.1 A deliberately unformatted commit, an unlisted-licence crate and a header drift each fail CI on a test branch
- [ ] 5.2 Second CI run is measurably faster than the first (cache hit)
- [ ] 5.3 Hardware (André): app built from this branch connects and edits a patch (no functional change expected)
- [ ] 5.4 `openspec validate harden-quality-gates`, merge into `main`, archive the change
