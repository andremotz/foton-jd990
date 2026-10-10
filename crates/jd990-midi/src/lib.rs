//! MIDI-Transport zum JD-990 (M2): Port öffnen, RQ1 senden, DT1 einsammeln,
//! schreiben mit Rücklese-Verifikation.
//!
//! Lehren aus M0 (internes Logbuch), die hier eingebaut sind:
//! * RQ1 immer mit **exakter** Blockgröße — Über-Anfragen liefern am Ende eine
//!   falsche Prüfsumme.
//! * Jede empfangene Nachricht wird per Prüfsumme validiert; ein Lesevorgang ist
//!   erst gültig, wenn der Block **lückenlos** da ist.
//! * Schreiben wird per Zurücklesen bestätigt — ein stiller Sendeverlust war in M0
//!   nur so zu finden.
//! * Geschrieben wird nur in den Temp-Bereich (ADR-7). Andere Bereiche verweigert
//!   dieses Crate grundsätzlich.
//! * Nach dem Patch-Common-Block (118 Byte) braucht das Gerät Zeit: folgen die
//!   Tone-Daten binnen ≤ 70 ms, geht u. a. eine Key-Range-Änderung verloren
//!   (gemessen: 70 ms 0/5, 80 ms 5/5). Deshalb `common_settle` = 150 ms.

pub mod queue;

use jd990_core::params::{PATCH_SIZE, PERF_SIZE};
use jd990_core::{Patch, Performance};
use jd990_core::sysex::{Address, Dt1, rq1};
use midir::{Ignore, MidiInput, MidiInputConnection, MidiOutput, MidiOutputConnection};
use std::sync::mpsc::{Receiver, channel};
use std::time::{Duration, Instant};

pub const DEFAULT_PORT: &str = "Unitor8/AMT8 Port 2";

#[derive(Debug)]
pub enum Error {
    PortNotFound(String),
    Midi(String),
    Timeout { address: Address, got: usize, want: usize },
    Protocol(String),
    Refused(Address),
    Mismatch { offsets: Vec<usize> },
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            Error::PortNotFound(p) => write!(f, "MIDI-Port „{p}“ nicht gefunden"),
            Error::Midi(e) => write!(f, "MIDI: {e}"),
            Error::Timeout { address, got, want } => write!(f, "Zeitüberschreitung bei {address}: {got}/{want} Byte"),
            Error::Protocol(e) => write!(f, "Protokoll: {e}"),
            Error::Refused(a) => write!(f, "VERWEIGERT: Schreiben außerhalb der freigegebenen Temp-Bereiche ({a})"),
            Error::Mismatch { offsets } => write!(f, "Rücklesen weicht ab an {} Offsets, z. B. {:?}", offsets.len(), &offsets[..offsets.len().min(8)]),
        }
    }
}

impl std::error::Error for Error {}

pub type Result<T> = std::result::Result<T, Error>;

/// Namen aller MIDI-Ausgänge (zur Diagnose).
pub fn output_ports() -> Vec<String> {
    let Ok(out) = MidiOutput::new("jd990") else { return vec![] };
    out.ports().iter().filter_map(|p| out.port_name(p).ok()).collect()
}

pub struct Device {
    out: MidiOutputConnection,
    _in: MidiInputConnection<()>,
    rx: Receiver<Vec<u8>>,
    pub device_id: u8,
    /// Pause zwischen DT1-Nachrichten. Handbuch: ≥ 20 ms bei Mehrteilern;
    /// M0 hat gezeigt, dass das Gerät auch 0 ms verkraftet (C2).
    pub dt1_gap: Duration,
    /// Pause nach dem Patch-Common-Block beim Senden eines ganzen Patches.
    pub common_settle: Duration,
}

impl Device {
    /// Öffnet Ein- und Ausgang mit exakt diesem Portnamen.
    pub fn open(port: &str, device_id: u8) -> Result<Device> {
        let midi_out = MidiOutput::new("jd990").map_err(|e| Error::Midi(e.to_string()))?;
        let out_port = midi_out
            .ports()
            .into_iter()
            .find(|p| midi_out.port_name(p).as_deref() == Ok(port))
            .ok_or_else(|| Error::PortNotFound(port.into()))?;
        let out = midi_out.connect(&out_port, "jd990-out").map_err(|e| Error::Midi(e.to_string()))?;

        let mut midi_in = MidiInput::new("jd990").map_err(|e| Error::Midi(e.to_string()))?;
        midi_in.ignore(Ignore::TimeAndActiveSense); // SysEx durchlassen
        let in_port = midi_in
            .ports()
            .into_iter()
            .find(|p| midi_in.port_name(p).as_deref() == Ok(port))
            .ok_or_else(|| Error::PortNotFound(port.into()))?;
        let (tx, rx) = channel();
        let conn = midi_in
            .connect(&in_port, "jd990-in", move |_, msg, _| {
                if msg.first() == Some(&0xF0) {
                    let _ = tx.send(msg.to_vec());
                }
            }, ())
            .map_err(|e| Error::Midi(e.to_string()))?;
        Ok(Device { out, _in: conn, rx, device_id, dt1_gap: Duration::from_millis(20), common_settle: Duration::from_millis(150) })
    }

    fn send(&mut self, msg: &[u8]) -> Result<()> {
        self.out.send(msg).map_err(|e| Error::Midi(e.to_string()))
    }

    /// Liest `size` Byte ab `address` (muss eine Basisadresse sein).
    pub fn read(&mut self, address: Address, size: usize) -> Result<Vec<u8>> {
        while self.rx.try_recv().is_ok() {} // Altlasten verwerfen
        self.send(&rq1(self.device_id, address, size))?;
        let deadline = Instant::now() + Duration::from_millis(1000 + size as u64 * 2);
        let mut buf = vec![None; size];
        let mut got = 0;
        while got < size {
            let wait = deadline.saturating_duration_since(Instant::now());
            let Ok(msg) = self.rx.recv_timeout(wait) else {
                return Err(Error::Timeout { address, got, want: size });
            };
            let dt1 = Dt1::decode(&msg).map_err(|e| Error::Protocol(e.to_string()))?;
            if dt1.device != self.device_id || dt1.address < address {
                continue;
            }
            let start = (dt1.address.0 - address.0) as usize;
            if start + dt1.data.len() > size {
                return Err(Error::Protocol(format!("DT1 bei {} ragt über den angefragten Block hinaus", dt1.address)));
            }
            for (i, b) in dt1.data.iter().enumerate() {
                if buf[start + i].replace(*b).is_none() {
                    got += 1;
                }
            }
        }
        Ok(buf.into_iter().map(Option::unwrap).collect())
    }

    /// Antwortet das Gerät an diesem Port mit dieser Device-ID? (Kein Identity
    /// Request — den beantwortet der JD-990 nicht; stattdessen der Patchname.)
    pub fn probe(&mut self) -> Result<String> {
        let name = self.read(Address::TEMP_PATCH, 16)?;
        Ok(String::from_utf8_lossy(&name).into_owned())
    }

    pub fn read_patch(&mut self, address: Address) -> Result<Patch> {
        let data = self.read(address, PATCH_SIZE)?;
        Ok(Patch::from_bytes(data.try_into().unwrap()))
    }

    /// Schreibfreigabe (ADR-7, ADR-22): Temp-Patch `03`, Temp-Performance `01`
    /// (0–143), Part-Patches `02 00…06` — und im System-Bereich nur `00 00 00 00`
    /// mit genau einem Byte PATCH (0) oder PERFORMANCE (1).
    pub fn allowed(address: Address, data: &[u8]) -> bool {
        let part = (address.0 >> 14) & 0x7F;
        let off = address.patch_offset();
        match address.area() {
            0x03 => off + data.len() <= PATCH_SIZE,
            0x02 => part <= 6 && off + data.len() <= PATCH_SIZE,
            0x01 => part == 0 && off + data.len() <= PERF_SIZE,
            0x00 => address.0 == 0 && data.len() == 1 && data[0] <= 1,
            _ => false,
        }
    }

    fn guard(address: Address) -> Result<()> {
        if address.area() == 0x03 { Ok(()) } else { Err(Error::Refused(address)) }
    }

    fn guard_data(address: Address, data: &[u8]) -> Result<()> {
        if Self::allowed(address, data) { Ok(()) } else { Err(Error::Refused(address)) }
    }

    /// Schreibt ein DT1 an eine freigegebene Adresse (Temp-Bereiche, System-Modus).
    pub fn write(&mut self, address: Address, data: &[u8]) -> Result<()> {
        Self::guard_data(address, data)?;
        let m = Dt1 { device: self.device_id, address, data: data.to_vec() };
        self.send(&m.encode())?;
        std::thread::sleep(self.dt1_gap);
        Ok(())
    }

    /// System-Modus lesen: 0 PATCH, 1 PERFORMANCE, 2 RHYTHM.
    pub fn mode(&mut self) -> Result<u8> {
        Ok(self.read(Address::SYSTEM, 38)?[0])
    }

    /// System-Modus setzen (nur PATCH/PERFORMANCE) und warten, bis das Gerät wieder
    /// antwortet. Liefert die Zeit bis zur ersten Antwort.
    pub fn set_mode(&mut self, mode: u8) -> Result<Duration> {
        let start = Instant::now();
        self.write(Address::SYSTEM, &[mode])?;
        for _ in 0..20 {
            std::thread::sleep(Duration::from_millis(100));
            if let Ok(m) = self.mode()
                && m == mode
            {
                return Ok(start.elapsed());
            }
        }
        Err(Error::Protocol(format!("Modus {mode} nach 2 s nicht bestätigt")))
    }

    pub fn read_performance(&mut self, address: Address) -> Result<Performance> {
        let data = self.read(address, PERF_SIZE)?;
        Ok(Performance::from_bytes(data.try_into().unwrap()))
    }

    /// Ganzer Patch an einen freigegebenen Patch-Block (`03` oder `02 0n`), mit Rücklese-Prüfung.
    pub fn write_patch_at(&mut self, base: Address, patch: &Patch) -> Result<()> {
        let once = |d: &mut Device, slow: u32| -> Result<()> {
            for m in patch.to_dt1(d.device_id, base) {
                Self::guard_data(m.address, &m.data)?;
                d.send(&m.encode())?;
                std::thread::sleep(if m.address.patch_offset() == 0 { d.common_settle } else { d.dt1_gap } * slow);
            }
            let back = d.read_patch(base)?;
            let offsets: Vec<usize> = (0..PATCH_SIZE).filter(|&i| back.as_bytes()[i] != patch.as_bytes()[i]).collect();
            if offsets.is_empty() { Ok(()) } else { Err(Error::Mismatch { offsets }) }
        };
        match once(self, 1) {
            Err(Error::Mismatch { .. }) => once(self, 4),
            r => r,
        }
    }

    /// Temp-Performance schreiben (ein DT1 mit 144 Byte), mit Rücklese-Prüfung.
    pub fn write_performance(&mut self, perf: &Performance) -> Result<()> {
        let data = perf.as_bytes().to_vec();
        Self::guard_data(Address::TEMP_PERFORMANCE, &data)?;
        let m = Dt1 { device: self.device_id, address: Address::TEMP_PERFORMANCE, data };
        self.send(&m.encode())?;
        std::thread::sleep(self.common_settle);
        let back = self.read_performance(Address::TEMP_PERFORMANCE)?;
        let offsets: Vec<usize> = (0..PERF_SIZE).filter(|&i| back.as_bytes()[i] != perf.as_bytes()[i]).collect();
        if offsets.is_empty() { Ok(()) } else { Err(Error::Mismatch { offsets }) }
    }

    /// Schreibt einen Patch in den Temp-Bereich und bestätigt per Zurücklesen.
    /// Weicht das Ergebnis ab, wird einmal mit vervierfachten Pausen wiederholt.
    pub fn write_temp_patch(&mut self, patch: &Patch) -> Result<()> {
        match self.write_temp_patch_once(patch, 1) {
            Err(Error::Mismatch { .. }) => self.write_temp_patch_once(patch, 4),
            r => r,
        }
    }

    fn write_temp_patch_once(&mut self, patch: &Patch, slow: u32) -> Result<()> {
        let msgs = patch.to_dt1(self.device_id, Address::TEMP_PATCH);
        for m in &msgs {
            Self::guard(m.address)?;
        }
        for m in msgs {
            self.send(&m.encode())?;
            let common = m.address.patch_offset() == 0;
            std::thread::sleep(if common { self.common_settle } else { self.dt1_gap } * slow);
        }
        let back = self.read_patch(Address::TEMP_PATCH)?;
        let offsets: Vec<usize> = (0..PATCH_SIZE).filter(|&i| back.as_bytes()[i] != patch.as_bytes()[i]).collect();
        if offsets.is_empty() { Ok(()) } else { Err(Error::Mismatch { offsets }) }
    }

    /// Schreibt Bytes an eine Temp-Adresse (z. B. ein Parameter aus `Patch::set`).
    pub fn write_temp(&mut self, address: Address, data: &[u8]) -> Result<()> {
        Self::guard(address)?;
        let m = Dt1 { device: self.device_id, address, data: data.to_vec() };
        self.send(&m.encode())?;
        std::thread::sleep(self.dt1_gap);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn schreibfreigabe_nur_temp_und_modus() {
        assert!(Device::allowed(Address::TEMP_PATCH, &[1, 2]));
        assert!(Device::allowed(Address::perf_patch(6).offset(485), &[1]));
        assert!(Device::allowed(Address::TEMP_PERFORMANCE.offset(0x64), &[50]));
        assert!(!Device::allowed(Address::TEMP_PERFORMANCE.offset(143), &[1, 2]), "über 144 hinaus");
        assert!(Device::allowed(Address::SYSTEM, &[1]));
        assert!(!Device::allowed(Address::SYSTEM, &[2]), "RHYTHM nie");
        assert!(!Device::allowed(Address(1), &[0]), "andere System-Adresse");
        assert!(!Device::allowed(Address(0x02 << 21 | 7 << 14), &[0]), "Part 8 hat keinen Patch-Block");
        assert!(!Device::allowed(Address::internal_patch(0), &[0]));
        assert!(!Device::allowed(Address::internal_performance(0), &[0]));
        assert!(!Device::allowed(Address(0x04 << 21), &[0]), "Rhythm");
    }
}
