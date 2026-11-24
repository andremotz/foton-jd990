# Spec Delta

## Purpose
Every SysEx write that leaves FotonJD990, from the app, the CLI or a future plugin hub,
is checked against the ADR-7 whitelist implemented once in the Rust core.

## ADDED Requirements

### Requirement: Single whitelist
The write-permission rule SHALL be implemented exactly once, in `jd990-core`. The
`jd990-midi` transport and the JUCE host MUST both call that implementation before
sending a DT1 message.

#### Scenario: Host sends a parameter change
- **WHEN** the host queues a DT1 for `03 00 01 10`
- **THEN** `DeviceLink` asks `jd990_dt1_allowed` and sends the message only if it returns true

### Requirement: Permitted addresses
The rule SHALL permit only: temp patch `03 00 …` within 486 bytes, temp performance
`01 00 …` within 144 bytes, part patches `02 00 …` to `02 06 …` within 486 bytes, and
`00 00 00 00` with exactly one data byte of value 0 or 1. All other writes MUST be refused.

#### Scenario: Temp patch block other than 0
- **WHEN** a DT1 targets `03 05 00 00`
- **THEN** the rule refuses it, in Rust and in the host

#### Scenario: Internal memory
- **WHEN** a DT1 targets an internal patch (`06 …`) or internal performance
- **THEN** the rule refuses it

#### Scenario: Malformed message
- **WHEN** a message has a wrong checksum, is not a Roland DT1 for the JD-990 (model 0x57),
  has no data, or contains a data byte ≥ 0x80
- **THEN** the rule refuses it

#### Scenario: Write past the end of a block
- **WHEN** a DT1 starts at temp performance offset 143 with two data bytes
- **THEN** the rule refuses it

### Requirement: Read requests cannot write
The host's read-request path SHALL accept only RQ1 messages. Any other message passed to it
MUST be refused and reported like a refused write.

#### Scenario: DT1 passed as a read request
- **WHEN** a DT1 message is passed to `DeviceLink::request`
- **THEN** nothing is sent and the refusal is reported

### Requirement: Shared test vectors
One table of allowed and refused messages SHALL be checked both by `cargo test` and by
the C smoke test against the FFI.

#### Scenario: Drift between implementations
- **WHEN** someone changes the rule so that a refused vector becomes allowed
- **THEN** `cargo test` and `crates/jd990-ffi/tests/c_smoke.sh` both fail

### Requirement: Refusals are visible
A refused write SHALL NOT be sent, and SHALL be logged and shown in the sync status,
also in release builds.

#### Scenario: Refused write in a release build
- **WHEN** the host is asked to send a DT1 to a refused address
- **THEN** nothing is sent
- **AND** the sync status names the refused address

#### Scenario: Refusal covered by a UI reference image
- **WHEN** the app runs with `--offline --testmode --init --refusetest`
- **THEN** the reference image `verweigert` shows the refusal in the sync status
