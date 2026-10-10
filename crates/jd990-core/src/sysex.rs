//! Roland-SysEx: RQ1/DT1-Rahmen, 7-Bit-Adressen, Prüfsumme.
//!
//! ```text
//! F0 41 <dev> 57 <cmd> <a3 a2 a1 a0> <data…> <sum> F7
//! ```
//! Adressen sind 4 × 7 Bit. Intern wird eine Adresse als lineare Zahl
//! geführt (`a3<<21 | a2<<14 | a1<<7 | a0`), damit Offsets addierbar sind.

use crate::params::MODEL_ID;
use std::fmt;

pub const ROLAND: u8 = 0x41;
pub const RQ1: u8 = 0x11;
pub const DT1: u8 = 0x12;
/// Maximale Nutzdaten je DT1 (MIDI Implementation, App. 43).
pub const DT1_MAX_DATA: usize = 256;
pub const DEFAULT_DEVICE: u8 = 0x10;

/// Lineare Adresse im 7-Bit-Raum des Geräts.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Address(pub u32);

impl Address {
    pub const SYSTEM: Address = Address(0);
    pub const TEMP_PERFORMANCE: Address = Address(0x01 << 21);
    pub const TEMP_PATCH: Address = Address(0x03 << 21);
    pub const INTERNAL_PATCH: Address = Address(0x06 << 21);

    /// Temp-Patch von Performance-Part `part` (0–6 ≙ Part 1–7), `02 0n 00 00`.
    pub fn perf_patch(part: u8) -> Address {
        assert!(part < 7, "Part {part} außerhalb 0–6");
        Address(0x02 << 21 | (part as u32) << 14)
    }

    /// Interne Performance `n` (0–15 ≙ 101–116), `05 nn 00 00`.
    pub fn internal_performance(n: u8) -> Address {
        assert!(n < 16, "Performance {n} außerhalb 0–15");
        Address(0x05 << 21 | (n as u32) << 14)
    }

    pub fn from_bytes(b: [u8; 4]) -> Address {
        Address(b.iter().fold(0, |acc, &x| (acc << 7) | (x & 0x7F) as u32))
    }

    pub fn to_bytes(self) -> [u8; 4] {
        [(self.0 >> 21) as u8 & 0x7F, (self.0 >> 14) as u8 & 0x7F, (self.0 >> 7) as u8 & 0x7F, self.0 as u8 & 0x7F]
    }

    /// Interner Patch `n` (0–63 ≙ I-11 … I-88).
    pub fn internal_patch(n: u8) -> Address {
        assert!(n < 64, "Patchnummer {n} außerhalb 0–63");
        Address(Self::INTERNAL_PATCH.0 | (n as u32) << 14)
    }

    pub fn offset(self, by: usize) -> Address {
        Address(self.0 + by as u32)
    }

    /// Oberstes Adressbyte (Bereich: 03 = Temp Patch, 06 = Internal Patch, …).
    pub fn area(self) -> u8 {
        self.to_bytes()[0]
    }

    /// Offset innerhalb eines Patch-Blocks (untere 14 Bit).
    pub fn patch_offset(self) -> usize {
        (self.0 & 0x3FFF) as usize
    }

    /// Lesbare Bezeichnung des Patch-Platzes: „Temp“, „I:11“ … „I:88“, „Card C:11“ …
    pub fn slot_label(self) -> String {
        let n = ((self.0 >> 14) & 0x7F) as u8;
        let slot = format!("{}{}", n / 8 + 1, n % 8 + 1);
        match self.area() {
            0x03 => "Temp".into(),
            0x02 => format!("Performance-Part {}", n + 1),
            0x06 => format!("I:{slot}"),
            0x0A => format!("Card C:{slot}"),
            a => format!("Bereich {a:02X}"),
        }
    }

    /// Basisadresse des Patch-Blocks, in dem diese Adresse liegt.
    pub fn patch_base(self) -> Address {
        Address(self.0 & !0x3FFF)
    }
}

impl fmt::Display for Address {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let b = self.to_bytes();
        write!(f, "{:02X} {:02X} {:02X} {:02X}", b[0], b[1], b[2], b[3])
    }
}

/// Roland-Prüfsumme über Adresse + Daten (bzw. Größe).
pub fn checksum(bytes: &[u8]) -> u8 {
    let sum: u32 = bytes.iter().map(|&b| b as u32).sum();
    ((128 - sum % 128) % 128) as u8
}

/// Eine dekodierte DT1-Nachricht.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Dt1 {
    pub device: u8,
    pub address: Address,
    pub data: Vec<u8>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SysexError {
    NotRoland,
    WrongModel(u8),
    NotDt1(u8),
    TooShort,
    Checksum { address: Address, expected: u8, found: u8 },
    DataHighBit(Address),
    Incomplete,
}

impl fmt::Display for SysexError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            SysexError::NotRoland => write!(f, "keine Roland-Nachricht"),
            SysexError::WrongModel(m) => write!(f, "Model-ID {m:#04x} statt {MODEL_ID:#04x}"),
            SysexError::NotDt1(c) => write!(f, "Kommando {c:#04x} ist kein DT1"),
            SysexError::TooShort => write!(f, "Nachricht zu kurz"),
            SysexError::Checksum { address, expected, found } => {
                write!(f, "Prüfsumme bei {address}: erwartet {expected:#04x}, gefunden {found:#04x}")
            }
            SysexError::DataHighBit(a) => write!(f, "Datenbyte ≥ 0x80 bei {a}"),
            SysexError::Incomplete => write!(f, "Daten unvollständig oder nicht zusammenhängend"),
        }
    }
}

impl std::error::Error for SysexError {}

impl Dt1 {
    /// Kodiert als vollständige SysEx-Nachricht.
    pub fn encode(&self) -> Vec<u8> {
        assert!(self.data.len() <= DT1_MAX_DATA, "DT1 mit {} Byte > {DT1_MAX_DATA}", self.data.len());
        let mut body = self.address.to_bytes().to_vec();
        body.extend_from_slice(&self.data);
        let mut msg = vec![0xF0, ROLAND, self.device, MODEL_ID, DT1];
        msg.extend_from_slice(&body);
        msg.push(checksum(&body));
        msg.push(0xF7);
        msg
    }

    /// Dekodiert eine einzelne Nachricht (inkl. F0 … F7).
    pub fn decode(msg: &[u8]) -> Result<Dt1, SysexError> {
        if msg.len() < 11 || msg[0] != 0xF0 || *msg.last().unwrap() != 0xF7 {
            return Err(SysexError::TooShort);
        }
        if msg[1] != ROLAND {
            return Err(SysexError::NotRoland);
        }
        if msg[3] != MODEL_ID {
            return Err(SysexError::WrongModel(msg[3]));
        }
        if msg[4] != DT1 {
            return Err(SysexError::NotDt1(msg[4]));
        }
        let address = Address::from_bytes([msg[5], msg[6], msg[7], msg[8]]);
        let body = &msg[5..msg.len() - 2];
        let found = msg[msg.len() - 2];
        let expected = checksum(body);
        if found != expected {
            return Err(SysexError::Checksum { address, expected, found });
        }
        let data = msg[9..msg.len() - 2].to_vec();
        if data.iter().any(|&b| b >= 0x80) {
            return Err(SysexError::DataHighBit(address));
        }
        Ok(Dt1 { device: msg[2], address, data })
    }
}

/// RQ1 für `size` Bytes ab `address`. Das Gerät antwortet nur auf Basisadressen
/// und nur mit exakter Blockgröße sauber (Logbuch 2026-09-27).
pub fn rq1(device: u8, address: Address, size: usize) -> Vec<u8> {
    let mut body = address.to_bytes().to_vec();
    body.extend_from_slice(&Address(size as u32).to_bytes());
    let mut msg = vec![0xF0, ROLAND, device, MODEL_ID, RQ1];
    msg.extend_from_slice(&body);
    msg.push(checksum(&body));
    msg.push(0xF7);
    msg
}

/// Zerlegt einen Bytestrom (z. B. eine .syx-Datei) in einzelne SysEx-Nachrichten.
/// Bytes außerhalb von F0…F7 werden ignoriert.
pub fn split_messages(bytes: &[u8]) -> Vec<&[u8]> {
    let mut out = Vec::new();
    let mut start = None;
    for (i, &b) in bytes.iter().enumerate() {
        match b {
            0xF0 => start = Some(i),
            0xF7 => {
                if let Some(s) = start.take() {
                    out.push(&bytes[s..=i]);
                }
            }
            _ => {}
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn pruefsumme_aus_dem_handbuch() {
        // Beispiel App. 50: 01 00 00 29 05 → 51H
        assert_eq!(checksum(&[0x01, 0x00, 0x00, 0x29, 0x05]), 0x51);
    }

    #[test]
    fn rq1_patchname_wie_am_geraet() {
        // Logbuch 2026-09-27: diese Bytes lieferten „Oasis"
        assert_eq!(
            rq1(0x10, Address::TEMP_PATCH, 16),
            [0xF0, 0x41, 0x10, 0x57, 0x11, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x6D, 0xF7]
        );
    }

    #[test]
    fn rq1_beispiele_aus_dem_handbuch() {
        // App. 53: EQ von Patch I-51 → F0 41 10 57 11 06 20 00 21 00 00 00 07 32 F7
        assert_eq!(
            rq1(0x10, Address::internal_patch(32).offset(0x21), 7),
            [0xF0, 0x41, 0x10, 0x57, 0x11, 0x06, 0x20, 0x00, 0x21, 0x00, 0x00, 0x00, 0x07, 0x32, 0xF7]
        );
        // App. 55: Tone B von Patch I-12 → F0 41 10 57 11 06 01 01 54 00 00 00 5C 48 F7
        // (Handbuch druckt „01 54“ für Tone B; laut Tabelle *4-3 liegt Tone B bei 01 52.
        //  Prüfsumme 48H passt zu 01 54 — ein Druckfehler im Beispiel, nicht in der Tabelle.)
        assert_eq!(checksum(&[0x06, 0x01, 0x01, 0x54, 0x00, 0x00, 0x00, 0x5C]), 0x48);
    }

    #[test]
    fn dt1_beispiel_aus_dem_handbuch() {
        // App. 53: Structure A&B von I-21 auf TYPE 3 → F0 41 10 57 12 06 08 00 28 02 48 F7
        let m = Dt1 { device: 0x10, address: Address::internal_patch(8).offset(0x28), data: vec![0x02] };
        assert_eq!(m.encode(), [0xF0, 0x41, 0x10, 0x57, 0x12, 0x06, 0x08, 0x00, 0x28, 0x02, 0x48, 0xF7]);
        assert_eq!(Dt1::decode(&m.encode()).unwrap(), m);
    }

    #[test]
    fn adressen_sind_linear_im_7_bit_raum() {
        let b = Address::TEMP_PATCH.offset(0xD2); // Tone B
        assert_eq!(b.to_bytes(), [0x03, 0x00, 0x01, 0x52]);
        assert_eq!(Address::from_bytes([0x03, 0x00, 0x03, 0x0A]).patch_offset(), 0x18A); // Tone D
        assert_eq!(Address::internal_patch(63).to_bytes(), [0x06, 0x3F, 0x00, 0x00]);
    }

    #[test]
    fn falsche_pruefsumme_wird_erkannt() {
        let mut m = Dt1 { device: 0x10, address: Address::TEMP_PATCH, data: vec![1, 2, 3] }.encode();
        let n = m.len();
        m[n - 2] ^= 1;
        assert!(matches!(Dt1::decode(&m), Err(SysexError::Checksum { .. })));
    }
}
