# Spec Delta

## Purpose
Every push and pull request to FotonJD990 is checked automatically for formatting,
lint, tests, dependency safety and C-ABI consistency, without needing a JD-990.

## ADDED Requirements

### Requirement: Formatting enforced
CI SHALL fail when `cargo fmt --all --check` reports a difference, using the repository's
`rustfmt.toml`.

#### Scenario: Unformatted pull request
- **WHEN** a pull request contains Rust code that rustfmt would change
- **THEN** the Rust job fails with the formatting diff in the log

### Requirement: Dependency checks
CI SHALL run `cargo deny check` and MUST fail on a known security advisory or on a
dependency licence that is not on the allow-list in `deny.toml`.

#### Scenario: New dependency with an unlisted licence
- **WHEN** a pull request adds a crate whose licence isn't on the allow-list
- **THEN** the Rust job fails and names the crate and licence

### Requirement: C header matches the Rust exports
CI SHALL fail when a function exported by `jd990-ffi` is missing from
`include/jd990.h` or declared there with a different signature.

#### Scenario: Parameter added in Rust only
- **WHEN** an exported function gains a parameter in `lib.rs` but not in `jd990.h`
- **THEN** the header check fails and names the function

### Requirement: UI reference images in CI
The macOS job SHALL run `host/tests/ui_snapshots.sh`. When the images differ, the job
MUST upload the diff images as a build artifact.

#### Scenario: Changed panel rendering
- **WHEN** a pull request changes how a fader is drawn without updating `host/tests/golden/`
- **THEN** the macOS job reports the scenario and the diff image can be downloaded
