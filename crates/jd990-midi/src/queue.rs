//! Sende-Warteschlange (ADR-8): Coalescing, Byte-Budget, Prioritäten.
//!
//! Reine Logik ohne MIDI und ohne Uhr — die Zeit wird hereingereicht, damit
//! alles deterministisch testbar ist. Der Transport fragt regelmäßig
//! `next(now)` und sendet, was herauskommt.
//!
//! * **Coalescing:** pro Zieladresse gilt nur der zuletzt gesetzte Wert
//!   („last value wins“). Ein Fader-Schwenk über 100 Werte erzeugt keinen Rückstau.
//! * **Byte-Budget:** Token-Bucket in Byte/s. Default 60 % von 3125 B/s, damit
//!   gleichzeitig gespielte Noten nicht ruckeln.
//! * **Prioritäten:** Einzelparameter vor Bulk (ganze Patches). Ein Bulk-Block
//!   wird nie von einem Parameter für dieselbe Adresse überholt — sonst würde
//!   der ältere Bulk-Wert den neueren Parameter überschreiben.

use jd990_core::sysex::{Address, Dt1};
use std::collections::VecDeque;
use std::time::Duration;

/// MIDI-DIN: 31 250 Baud, 10 Bit je Byte.
pub const MIDI_BYTES_PER_SEC: f64 = 3125.0;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Priority {
    Param,
    Bulk,
}

#[derive(Debug)]
struct Entry {
    address: Address,
    data: Vec<u8>,
    pause_after: Duration,
}

impl Entry {
    fn wire_len(&self) -> usize {
        self.data.len() + 11 // F0 41 dev 57 12 + 4 Adresse + Daten + Summe + F7
    }
    fn covers(&self, a: Address) -> bool {
        a >= self.address && a.0 < self.address.0 + self.data.len() as u32
    }
}

pub struct SendQueue {
    device: u8,
    params: VecDeque<Entry>,
    bulk: VecDeque<Entry>,
    bytes_per_sec: f64,
    burst: f64,
    tokens: f64,
    last: Option<Duration>,
    blocked_until: Duration,
}

impl SendQueue {
    /// `share` = Anteil der MIDI-Bandbreite (0 < share ≤ 1), ADR-8: 0.6.
    pub fn new(device: u8, share: f64) -> SendQueue {
        let bytes_per_sec = MIDI_BYTES_PER_SEC * share.clamp(0.01, 1.0);
        let burst = 300.0; // ein 256-Byte-DT1 muss durchpassen
        SendQueue { device, params: VecDeque::new(), bulk: VecDeque::new(), bytes_per_sec, burst, tokens: burst, last: None, blocked_until: Duration::ZERO }
    }

    /// Einzelparameter. Liegt für dieselbe Adresse schon ein Wert in der
    /// Warteschlange, wird er ersetzt und behält seinen Platz.
    pub fn set(&mut self, address: Address, data: Vec<u8>) {
        if let Some(e) = self.params.iter_mut().find(|e| e.address == address) {
            e.data = data;
        } else {
            self.params.push_back(Entry { address, data, pause_after: Duration::ZERO });
        }
    }

    /// Bulk-Nachricht (z. B. ein Teil eines Patches) mit Pause danach
    /// (Patch Common: `common_settle`).
    pub fn bulk(&mut self, address: Address, data: Vec<u8>, pause_after: Duration) {
        self.bulk.push_back(Entry { address, data, pause_after });
    }

    pub fn is_empty(&self) -> bool {
        self.params.is_empty() && self.bulk.is_empty()
    }

    pub fn len(&self) -> usize {
        self.params.len() + self.bulk.len()
    }

    /// Nächste sendefertige Nachricht zum Zeitpunkt `now` (monoton), oder None,
    /// wenn das Budget gerade erschöpft ist oder eine Pause läuft.
    pub fn next(&mut self, now: Duration) -> Option<Vec<u8>> {
        if let Some(last) = self.last {
            let dt = now.saturating_sub(last).as_secs_f64();
            self.tokens = (self.tokens + dt * self.bytes_per_sec).min(self.burst);
        }
        self.last = Some(now);
        if now < self.blocked_until {
            return None;
        }
        // Parameter zuerst — außer ein wartender Bulk-Block deckt dieselbe Adresse
        // ab: dann muss der Bulk zuerst raus, sonst überschreibt er den neueren Wert.
        let from_params = match self.params.front() {
            Some(p) => !self.bulk.iter().any(|b| b.covers(p.address)),
            None => false,
        };
        let queue = if from_params { &mut self.params } else { &mut self.bulk };
        let need = queue.front()?.wire_len() as f64;
        if self.tokens < need {
            return None;
        }
        let e = queue.pop_front().unwrap();
        self.tokens -= need;
        self.blocked_until = now + e.pause_after;
        Some(Dt1 { device: self.device, address: e.address, data: e.data }.encode())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ms(n: u64) -> Duration {
        Duration::from_millis(n)
    }
    const CUTOFF_A: Address = Address((0x03 << 21) + 0x76 + 25);

    #[test]
    fn fader_schwenk_wird_zusammengefasst() {
        let mut q = SendQueue::new(0x10, 0.6);
        for v in 0..=100 {
            q.set(CUTOFF_A, vec![v]);
        }
        assert_eq!(q.len(), 1);
        let msg = q.next(ms(0)).unwrap();
        assert_eq!(msg[9], 100, "nur der letzte Wert");
        assert!(q.next(ms(1)).is_none());
    }

    #[test]
    fn byte_budget_wird_eingehalten() {
        let mut q = SendQueue::new(0x10, 0.6); // 1875 B/s
        for i in 0..200u32 {
            q.set(Address(CUTOFF_A.0 + (i % 60)), vec![1]);
            q.set(Address((0x03 << 21) + i), vec![1]);
        }
        let (mut sent, mut t) = (0usize, 0u64);
        while t <= 1000 {
            while let Some(m) = q.next(ms(t)) {
                sent += m.len();
            }
            t += 1;
        }
        // In 1 s höchstens Budget + Anfangs-Burst
        assert!(sent as f64 <= 1875.0 + 300.0 + 12.0, "{sent} Byte in 1 s");
        assert!(sent > 1500, "Budget wird auch ausgeschöpft: {sent}");
    }

    #[test]
    fn parameter_ueberholen_bulk() {
        let mut q = SendQueue::new(0x10, 1.0);
        q.bulk(Address(0x06 << 21), vec![0; 256], ms(0));
        q.set(CUTOFF_A, vec![50]);
        let first = q.next(ms(0)).unwrap();
        assert_eq!(first.len(), 12, "Parameter zuerst");
    }

    #[test]
    fn parameter_ueberholt_keinen_bulk_fuer_dieselbe_adresse() {
        let mut q = SendQueue::new(0x10, 1.0);
        q.bulk(Address(0x03 << 21).offset(0x76), vec![0; 256], ms(0)); // enthält Cutoff A
        q.set(CUTOFF_A, vec![50]);
        let first = q.next(ms(0)).unwrap();
        assert_eq!(first.len(), 256 + 11, "Bulk muss zuerst, sonst überschreibt er den neueren Wert");
        let later = (1..500).find_map(|t| q.next(ms(t))).unwrap();
        assert_eq!(later[9], 50);
    }

    #[test]
    fn pause_nach_common_block() {
        let mut q = SendQueue::new(0x10, 1.0);
        q.bulk(Address(0x03 << 21), vec![0; 118], ms(150));
        q.bulk(Address(0x03 << 21).offset(118), vec![0; 100], ms(0));
        assert!(q.next(ms(0)).is_some());
        assert!(q.next(ms(100)).is_none(), "Pause läuft");
        assert!(q.next(ms(151)).is_some());
    }
}
