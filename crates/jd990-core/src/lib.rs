//! Datenmodell und SysEx-Codec für den Roland JD-990.
//!
//! Kennt weder MIDI-Ports noch UI (ADR-1): Bytes rein, Bytes raus. Die
//! Parametertabellen werden aus `spec/params.toml` generiert (ADR-3).

pub mod display;
pub mod jd800;
pub mod morph;
pub mod params;
pub mod patch;
pub mod performance;
pub mod rules;
pub mod smf;
pub mod sysex;

pub use params::{ParamDef, Tone};
pub use patch::Patch;
pub use performance::{Performance, Setup};
