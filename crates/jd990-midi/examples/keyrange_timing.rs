//! Diagnose (Logbuch 2026-09-28): Geht eine Key-Range-Änderung verloren, wenn
//! direkt danach ein weiteres DT1 kommt? Schreibt nur in den Temp Patch und
//! stellt den Ausgangszustand am Ende wieder her.
use jd990_core::sysex::Address;
use jd990_midi::{DEFAULT_PORT, Device};
use std::time::Duration;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut d = Device::open(DEFAULT_PORT, 0x10)?;
    let original = d.read_patch(Address::TEMP_PATCH)?;
    let level = original.as_bytes()[0x10];
    for gap in [0u64, 10, 20, 40, 60, 80, 120] {
        let mut ok = 0;
        for _ in 0..5 {
            d.dt1_gap = Duration::from_millis(20);
            d.write_temp(Address::TEMP_PATCH.offset(0x2B), &[0])?;   // lo_b = 0
            d.write_temp(Address::TEMP_PATCH.offset(0x2F), &[127])?; // hi_b = 127
            std::thread::sleep(Duration::from_millis(200));
            d.dt1_gap = Duration::from_millis(gap);
            d.write_temp(Address::TEMP_PATCH.offset(0x2F), &[116])?; // hi_b = 116 …
            d.write_temp(Address::TEMP_PATCH.offset(0x10), &[level])?; // … direkt danach Patch Level (unverändert)
            std::thread::sleep(Duration::from_millis(200));
            if d.read_patch(Address::TEMP_PATCH)?.as_bytes()[0x2F] == 116 {
                ok += 1;
            }
        }
        println!("Einzel-DT1 hi_b, danach nach {gap:3} ms ein weiteres DT1: {ok}/5 übernommen");
    }
    d.dt1_gap = Duration::from_millis(150);
    d.write_temp_patch(&original)?;
    println!("Ausgangszustand wiederhergestellt ✓");
    Ok(())
}
