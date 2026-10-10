# 02 — JD-990 SysEx: Wissensstand

Dieses Dokument trennt strikt zwischen **gesichert**, **plausibel** und
**zu verifizieren**. Alles unter „zu verifizieren" ist Arbeitsgegenstand von
Meilenstein M0 und darf bis dahin nicht implementiert werden.

## ✅ Gesichert

### Nachrichtenrahmen

Roland-Standardformat:

```
F0  41  <dev>  57  <cmd>  <a3> <a2> <a1> <a0>  <data…>  <sum>  F7
 │   │    │    │     │     └────────────────┘              │
 │   │    │    │     │       4-Byte-Adresse                Prüfsumme
 │   │    │    │     └─ 11 = RQ1 (Request), 12 = DT1 (Data Set)
 │   │    │    └─────── Model ID JD-990 = 0x57
 │   │    └──────────── Device ID, Default 0x10 (= Unit #17)
 │   └───────────────── Roland Manufacturer ID
 └───────────────────── SysEx Start
```

* **Model ID `0x57`** — bestätigt durch Rolands eigenen Support-Artikel
  „JD-990: Turning Parts on or off using system exclusive".
* **Prüfsumme** — Roland-Standard: `sum = (128 - ((a3+a2+a1+a0+data…) mod 128)) mod 128`
* **Kommandos** — `RQ1 = 0x11`, `DT1 = 0x12`. Handshake-Kommandos (WSD/ACK/EOD/…)
  werden nicht benötigt; der einseitige RQ1/DT1-Betrieb genügt.

### Bekannte Adressbereiche (oberstes Adressbyte)

| `a3` | Bereich |
|---|---|
| `00` | System Setup |
| `01` | Temporary Performance |
| `03` | Temporary Patch / Patch Memory |

Belegt durch dokumentierte Request-Header und Rolands Beispiel
`F0 41 10 57 12 01 00 00 30 00 4F F7` (Performance-Part schalten).

### Datenstrukturen

Vollständig reverse-engineered und in der Praxis erprobt in
[JDTools](https://github.com/sagamusix/JDTools) (`JD-990.hpp`, BSD-3-Clause,
© Johannes Schultz). Feldreihenfolge = SysEx-Byte-Reihenfolge.

**`Tone990` — 92 Bytes, 4× pro Patch (Tone A/B/C/D):**

| Block | Bytes | Inhalt |
|---|---|---|
| WG | 14 | waveSource, waveform MSB/LSB, FXM Color/Depth, Sync Slave, Tone Delay Mode/Time, Pitch Coarse/Fine/Random, KeyFollow, Env Depth, Bender Sw |
| Pitch Env | 10 | Velo, Time-Velo, Time-KF, L0, T1, L1, T2, Sustain, T3, L3 |
| TVF | 5 | Filter Mode, Cutoff, Resonance, KeyFollow, Env Depth |
| TVF Env | 11 | Velo, Time-Velo, Time-KF, T1..T4, L1/L2/L4, Sustain |
| TVA | 6 | Level, Bias Direction/Point/Level, Pan, Pan KeyFollow |
| TVA Env | 10 | Velo, Time-Velo, Time-KF, T1..T4, L1/L2, Sustain |
| Common | 2 | Velocity Curve, Hold Control |
| LFO 1 | 9 | Waveform, Rate, Delay, Fade, Offset, Key Trigger, Depth Pitch/TVF/TVA |
| LFO 2 | 9 | dito |
| Control Source 1 | 8 | 4× (Destination, Depth) |
| Control Source 2 | 8 | dito |
| **Summe** | **92** | |

**`Patch990` — 486 Bytes:**

| Block | Bytes | Inhalt |
|---|---|---|
| Common | 26 | Name (16 ASCII), Level, Pan, Analog Feel, Voice Priority, Bend Down/Up, Tone Ctrl Src 1/2, Layer Tone, Active Tone |
| Key Effects | 7 | Portamento Sw/Mode/Type/Time, Solo Sw/Legato/Sync Master |
| EQ | 7 | Low Freq/Gain, Mid Freq/Q/Gain, High Freq/Gain |
| Structure | 2 | Structure A/B, Structure C/D (Typ 1–6) |
| Key Ranges | 8 | Low/High je Tone |
| Velocity | 12 | Range/Point/Fade je Tone |
| Effects | 55 | siehe unten |
| Octave Switch | 1 | |
| Tone A–D | 368 | 4 × 92 |
| **Summe** | **486** | |

**Effekt-Sektion (55 Bytes)** — hier unterscheidet sich der JD-990 deutlich
vom JD-800 und ist der Grund, warum JD-800-Panels nicht einfach passen:

* Group-A-Kette: frei sequenzierbar (`groupAsequence`) über 4 Block-Switches
  → Distortion (3), Phaser (5), Spectrum (7), Enhancer (2)
* Group-B-Kette: `groupBsequence` + 3 Block-Switches
  → Chorus (5), Delay (11), Reverb (6)
* Effect Balance Group B + 2× Control Source/Destination/Depth (7)
* **Delay ist 990-exklusiv erweitert**: Mode + 3 unabhängige Taps
  (Center/Left/Right) mit je MSB/LSB-Zeit und Level, plus Feedback

**`SpecialSetup990` (Rhythm) — ca. 6.524 Bytes:**
Common (23) + EQ (7) + Effect (28) + 61 Keys × (Name 10 + 4 + `Tone990` 92 = 106).

### Werteauflösung

Werte > 127 werden über **MSB/LSB-Bytepaare** übertragen (Waveform-Nummer,
Delay-Tap-Zeiten), nicht über 4-Bit-Nibbles. Alle übrigen Parameter sind
1 Byte à 0–127.

## ✅ M0-Fragen — beantwortet (2026-09-27/28)

| # | Frage | Antwort | Beleg |
|---|---|---|---|
| 1 | Adresstabelle | Vollständig → [`spec/addresses.toml`](../spec/addresses.toml). Temp Patch `03 00 00 00`, Internal Patch *n* `06 nn 00 00`, Rhythm `04`/`07`. Adressen linear im 7-Bit-Raum, keine Lücken innerhalb eines Blocks | Handbuch App. 50–56, JDTools, Hardware |
| 2 | Chunk-Größe | Max. 256 Datenbytes; das Gerät schneidet zusätzlich an Strukturgrenzen: Patch = 118 + 256 + 112 | Hardware |
| 3 | Frontplatten-Edits | **Werden nicht gesendet.** Keine Bidirektionalität → „Neu einlesen“ + ehrlicher Sync-Status | Hardware (2 Mitschnitte), Handbuch App. 49 |
| 4 | Timing-Grenzen | Keine Verluste bis Leitungsgeschwindigkeit (0 ms Abstand) | Hardware |
| 5 | `waveSource` = Card | Codec reicht unbekannte Werte byte-exakt durch (Test `unbekannte_werte_laufen_unveraendert_durch`); Hardware-Test mit echter Karte entfällt (out of scope) | Rust-Test |
| 6 | Device ID | `10H–1FH` = Unit# − 1, Default 17 → `0x10` | Handbuch App. 49, Hardware |
| 7 | Identity Request | **Keine Antwort.** Erkennung über RQ1 (z. B. Patchname) | Hardware |

Zusätzlich gefunden:
* RQ1 wird nur an **Basisadressen** und mit **size > 1** beantwortet (App. 49);
  **Über-Anfragen** liefern am Ende eine falsche Prüfsumme (Hardware).
* **„#“-Adressen** (LSB von Zwei-Byte-Parametern: Waveform, Delay-Taps) dürfen
  nicht Startadresse eines DT1 sein — Paare immer ab MSB schreiben (App. 50).
* Parameter-Abhängigkeiten im Gerät: siehe HIL-Sweep im Logbuch (internes Logbuch).

## ❓ Ursprünglich zu verifizieren in M0 (historisch)

Diese Punkte sind **die gesamte verbleibende Unbekannte** des Projekts:

1. **Vollständige Adresstabelle.** Die Struktur ist bekannt, die konkreten
   4-Byte-Adressen je Block noch nicht. Quelle: Owner's Manual, Anhang
   „MIDI Implementation" (Roland stellt die Manuals als PDF bereit).
   Konkret zu klären:
   - Adresse Temporary Patch (vermutlich `03 00 00 00`)
   - Adresse Patch Memory Internal/Card, Schrittweite je Patchnummer
   - Adresse Temporary Special Setup / Rhythm
   - Adresse System Setup und Temporary Performance Parts
   - Ob Adressen „lückenhaft" sind (Roland nutzt oft nur 7 Bit pro Byte und
     springt an Blockgrenzen)
2. **Chunk-Größe bei Bulk-Dumps.** Roland teilt große Dumps typischerweise in
   Pakete à 128 oder 256 Datenbytes. Ein 486-Byte-Patch käme dann als
   mehrere DT1-Nachrichten. Muss aus einem echten Dump abgelesen werden.
3. **Sendet der JD-990 Parameteränderungen der Frontplatte?**
   Entscheidet, ob bidirektionale Synchronisation möglich ist oder ob es einen
   „Vom Gerät neu einlesen"-Button braucht.
4. **Timing-Grenzen.** Wie schnell verträgt das Gerät aufeinanderfolgende
   DT1-Nachrichten, bevor es Nachrichten verwirft? Empirisch zu ermitteln.
5. **Verhalten bei `waveSource` = Card.** Patches, die Karten-Wellenformen
   nutzen, müssen **byteidentisch** durchlaufen, auch wenn der Editor den
   Namen nicht kennt (Erweiterungskarten sind out of scope — das Datenmodell
   darf sie trotzdem nicht beschädigen).
6. **Device ID.** Default `0x10`; am Gerät einstellbar. Editor muss das
   konfigurierbar machen und `0x7F` (Broadcast) für Discovery nutzen können.
7. **Identity Request** (`F0 7E <dev> 06 01 F7`) — antwortet der JD-990?
   Wäre die saubere Geräteerkennung.

## Ableitung: Bandbreitenbudget

MIDI-DIN läuft mit 31.250 Baud = **3.125 Bytes/s**.

| Vorgang | Größe | Dauer |
|---|---|---|
| Ein Parameter (DT1, 1 Byte Daten) | 11 Bytes | ≈ 3,5 ms |
| Ein Patch-Dump (486 Bytes + Overhead) | ≈ 500 Bytes | ≈ 160 ms |
| Ein Bank-Dump (64 Patches) | ≈ 32 kB | **≈ 10 s** |
| Special Setup (Rhythm) | ≈ 6,6 kB | ≈ 2,1 s |

**Konsequenzen für das Design:**
* Fader-Bewegungen müssen **koalesziert** werden (last-value-wins pro Parameter,
  Senderate begrenzt auf ~25 Nachrichten/s)
* Globales Byte-Budget: nie mehr als ~60 % der Bandbreite belegen, damit
  gleichzeitig gespielte Noten nicht ruckeln
* Bank-Dumps brauchen einen Fortschrittsbalken und dürfen die UI nicht blockieren

## Quellen

- [JDTools — sagamusix (BSD-3-Clause)](https://github.com/sagamusix/JDTools) — Datenstrukturen
- [Roland Support: JD-990 Turning Parts on or off using SysEx](https://support.roland.com/hc/en-us/articles/201938659-JD-990-Turning-Parts-on-or-off-using-system-exclusive) — Model ID, Adressbeispiel
- [SysExDB — Roland JD-990](https://www.sysexdb.com/roland/jd-990) — Request-Header der Hauptbereiche
- [Wikipedia — Roland JD-990](https://en.wikipedia.org/wiki/Roland_JD-990) — Gerätespezifikation
- [donsolaris.com — JD-990 Resource Centre](https://www.donsolaris.com/?p=585) — Sammlung (im Browser aufrufen, blockt automatisierte Zugriffe)
