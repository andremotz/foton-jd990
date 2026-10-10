//! SysEx aus Standard-MIDI-Files (.mid) holen — das historisch gängigste
//! Verteilformat für Bank-Dumps (ADR-5).
//!
//! Liest alle Tracks, folgt Running Status und sammelt SysEx-Ereignisse
//! (`F0 <len> …` und Fortsetzungen `F7 <len> …`) als vollständige `F0 … F7`-Nachrichten.
//! Alles andere (Noten, Meta-Events) wird übersprungen.

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SmfError {
    NotSmf,
    Truncated,
    BadEvent(u8),
}

impl std::fmt::Display for SmfError {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            SmfError::NotSmf => write!(f, "kein Standard-MIDI-File (MThd fehlt)"),
            SmfError::Truncated => write!(f, "Datei abgeschnitten"),
            SmfError::BadEvent(b) => write!(f, "unerwartetes Byte {b:#04x} ohne Running Status"),
        }
    }
}

impl std::error::Error for SmfError {}

pub fn is_smf(bytes: &[u8]) -> bool {
    bytes.starts_with(b"MThd")
}

struct Reader<'a> {
    b: &'a [u8],
    pos: usize,
}

impl<'a> Reader<'a> {
    fn take(&mut self, n: usize) -> Result<&'a [u8], SmfError> {
        let s = self.b.get(self.pos..self.pos + n).ok_or(SmfError::Truncated)?;
        self.pos += n;
        Ok(s)
    }
    fn u8(&mut self) -> Result<u8, SmfError> {
        Ok(self.take(1)?[0])
    }
    fn u32(&mut self) -> Result<u32, SmfError> {
        Ok(u32::from_be_bytes(self.take(4)?.try_into().unwrap()))
    }
    fn varlen(&mut self) -> Result<usize, SmfError> {
        let mut v = 0usize;
        for _ in 0..4 {
            let b = self.u8()?;
            v = (v << 7) | (b & 0x7F) as usize;
            if b & 0x80 == 0 {
                return Ok(v);
            }
        }
        Err(SmfError::Truncated)
    }
    fn done(&self) -> bool {
        self.pos >= self.b.len()
    }
}

/// Alle SysEx-Nachrichten eines SMF als zusammenhängender `.syx`-Bytestrom.
pub fn extract_sysex(bytes: &[u8]) -> Result<Vec<u8>, SmfError> {
    if !is_smf(bytes) {
        return Err(SmfError::NotSmf);
    }
    let mut r = Reader { b: bytes, pos: 0 };
    let mut out = Vec::new();
    while !r.done() {
        let id = r.take(4)?;
        let len = r.u32()? as usize;
        let chunk = r.take(len)?;
        if id == b"MTrk" {
            track(chunk, &mut out)?;
        }
    }
    Ok(out)
}

fn track(chunk: &[u8], out: &mut Vec<u8>) -> Result<(), SmfError> {
    let mut r = Reader { b: chunk, pos: 0 };
    let mut running: Option<u8> = None;
    let mut open = false; // eine F0-Nachricht wartet auf Fortsetzung (F7-Paket)
    while !r.done() {
        r.varlen()?; // Delta-Zeit
        let first = r.u8()?;
        match first {
            0xFF => {
                r.u8()?;
                let n = r.varlen()?;
                r.take(n)?;
            }
            0xF0 => {
                let n = r.varlen()?;
                let data = r.take(n)?;
                out.push(0xF0);
                out.extend_from_slice(data);
                open = data.last() != Some(&0xF7);
                running = None;
            }
            0xF7 => {
                // Fortsetzungspaket (oder „escaped“ Rohbytes)
                let n = r.varlen()?;
                let data = r.take(n)?;
                if open {
                    out.extend_from_slice(data);
                    open = data.last() != Some(&0xF7);
                }
                running = None;
            }
            b if b & 0x80 != 0 => {
                running = Some(b);
                r.take(channel_len(b))?;
            }
            _ => {
                // Running Status: `first` ist bereits das erste Datenbyte
                let s = running.ok_or(SmfError::BadEvent(first))?;
                r.take(channel_len(s) - 1)?;
            }
        }
    }
    Ok(())
}

fn channel_len(status: u8) -> usize {
    match status & 0xF0 {
        0xC0 | 0xD0 => 1,
        _ => 2,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn varlen(mut v: usize) -> Vec<u8> {
        let mut out = vec![(v & 0x7F) as u8];
        v >>= 7;
        while v > 0 {
            out.insert(0, 0x80 | (v & 0x7F) as u8);
            v >>= 7;
        }
        out
    }

    /// Baut ein SMF Typ 0 mit Noten (Running Status), Meta-Events und SysEx.
    fn smf(sysex: &[&[u8]]) -> Vec<u8> {
        let mut trk = vec![0x00, 0xFF, 0x03, 0x04, b'T', b'e', b's', b't'];
        trk.extend([0x00, 0x90, 0x3C, 0x40, 0x10, 0x3E, 0x40]); // Note On + Running Status
        for m in sysex {
            trk.push(0x00);
            trk.push(0xF0);
            trk.extend(varlen(m.len() - 1));
            trk.extend(&m[1..]);
        }
        trk.extend([0x00, 0xC0, 0x05, 0x00, 0xFF, 0x2F, 0x00]);
        let mut f = b"MThd".to_vec();
        f.extend([0, 0, 0, 6, 0, 0, 0, 1, 0, 96]);
        f.extend(b"MTrk");
        f.extend((trk.len() as u32).to_be_bytes());
        f.extend(trk);
        f
    }

    #[test]
    fn sysex_aus_smf() {
        let a: &[u8] = &[0xF0, 0x41, 0x10, 0x57, 0x12, 0x03, 0, 0, 0, 0x4F, 0x2E, 0xF7];
        let b: Vec<u8> = [&[0xF0, 0x41, 0x10, 0x57, 0x12][..], &[0x55; 300], &[0x00, 0xF7]].concat();
        let got = extract_sysex(&smf(&[a, &b])).unwrap();
        assert_eq!(got, [a, &b].concat());
    }

    #[test]
    fn fortsetzungspakete() {
        // F0 <n> 41 10 … (ohne F7) + F7 <n> … F7
        let mut trk = vec![0x00, 0xF0, 0x03, 0x41, 0x10, 0x57, 0x05, 0xF7, 0x03, 0x12, 0x00, 0xF7];
        trk.extend([0x00, 0xFF, 0x2F, 0x00]);
        let mut f = b"MThd".to_vec();
        f.extend([0, 0, 0, 6, 0, 0, 0, 1, 0, 96]);
        f.extend(b"MTrk");
        f.extend((trk.len() as u32).to_be_bytes());
        f.extend(trk);
        assert_eq!(extract_sysex(&f).unwrap(), [0xF0, 0x41, 0x10, 0x57, 0x12, 0x00, 0xF7]);
    }

    #[test]
    fn kein_smf() {
        assert_eq!(extract_sysex(&[0xF0, 0xF7]), Err(SmfError::NotSmf));
    }
}
