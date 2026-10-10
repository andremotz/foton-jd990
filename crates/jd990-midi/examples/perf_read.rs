//! Diagnose: Performance 1 (572 Byte) lesen und die Rohnachrichten zeigen.
use jd990_core::sysex::{Address, rq1};
fn main() {
    let msg = rq1(0x10, Address(0x05 << 21), 572);
    println!("RQ1: {:02X?}", msg);
    let mut d = jd990_midi::Device::open(jd990_midi::DEFAULT_PORT, 0x10).unwrap();
    for _ in 0..3 {
        match d.read(Address(0x05 << 21), 572) {
            Ok(v) => println!("ok {} Byte", v.len()),
            Err(e) => println!("Fehler: {e}"),
        }
    }
}
