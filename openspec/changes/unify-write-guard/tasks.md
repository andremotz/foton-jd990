# Tasks

Branch: `fix/unify-write-guard`

## 1. Core rule and test vectors

- [x] 1.1 `jd990_core::sysex::write_allowed(address, data)` and `write_allowed_bytes(&[u8])` per ADR-7 table
- [x] 1.2 Test vector table (allowed/refused), including `03 05 00 00`, perf offset 143 + 2 bytes, part 8 (`02 07`), system mode value 2, rhythm (`04`), internal patch/performance
- [x] 1.3 `jd990-midi` `Device::allowed` delegates to the core; existing test `schreibfreigabe_nur_temp_und_modus` still passes

## 2. FFI and host

- [x] 2.1 `jd990_dt1_allowed` in `jd990-ffi` and `include/jd990.h`
- [x] 2.2 Test vectors exported as a C header; `smoke.c` checks every vector through the FFI
- [x] 2.3 `DeviceLink::allowedDt1` calls `jd990_dt1_allowed`; the hand-written switch is removed
- [x] 2.4 Refused writes: counter + log line + sync-status text in release builds; test hook `--refusetest` + reference image `verweigert`
- [x] 2.5 `DeviceLink::request` accepts only RQ1 messages; anything else is refused and reported

## 3. Documentation

- [ ] 3.1 ADR-7 addendum: where the rule lives (done); ADR-27 status → accepted (on André's acceptance)
- [x] 3.2 README "Safety first": the whitelist lives in the core and is checked in both the CLI and the app
- [x] 3.3 `add-plugin-mode` task 3.1: hub uses `jd990_dt1_allowed`, no own copy

## 4. Acceptance before merge

- [x] 4.1 `cargo test`, `cargo clippy --all-targets -- -D warnings`, `crates/jd990-ffi/tests/c_smoke.sh` green
- [ ] 4.2 UI reference images unchanged (`host/tests/ui_snapshots.sh`)
- [ ] 4.3 Hardware (André): edit a patch, switch to performance mode, edit a part and send a setup; device follows, no refusal in the sync status
- [ ] 4.4 Hardware: `jd990 hil-sweep` over one range, 0 deviations
- [ ] 4.5 `openspec validate unify-write-guard`, merge into `main`, archive the change
