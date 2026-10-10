# 11 — Plan: Performance-Modus

Entscheidung und Begründung: [ADR-22](adr/0022-performance-modus.md).
Anforderungen: `AN-F1` … `AN-F9` in [10-anforderungen.md](10-anforderungen.md).
Stand: 2026-10-03 — P0–P3 erledigt und am Gerät erprobt; offen: P4 (Bibliothek für Setups), P5 (Abnahme André).

## Was das Gerät bietet (Handbuch App. 50–52, Pfom-1 … Pfom-10)

**Performance Common (48 Byte, `01 00 00 00`)**
* Name (16)
* Rx-Kanäle Patch-/Rhythm-Modus, Control-Kanal
* Empfangsschalter (Program Change, Volume, Bender, Aftertouch, Modulation, Breath, Expression, Foot)
* Sync-Part (NONE, PART1–7)
* Stimmenreservierung Part 1–8 (je 0–24)
* Effekt-Schalter Chorus/Delay/Reverb/Gruppe A
* Chorus, Delay, Reverb der Performance

Die genaue Byte-Reihenfolge ist aus dem OCR nicht sicher ablesbar. Sie wird in P0
gegen das Handbuch-PDF und am Gerät festgelegt.

**Part 1–8 (je 12 Byte, ab `01 00 00 30`, Abstand 12)**

| Offset | Parameter | Werte |
|---|---|---|
| 00 | MIDI Rx Switch | OFF/ON |
| 01 | MIDI Rx Channel | 1–16 |
| 02 | Bank | INT/Card, Preset A/B |
| 03 | Patch(/Rhythm) Number | 1–128 |
| 04 | Part Level | 0–100 |
| 05 | Part Pan | L50–50R |
| 06 | Part Pitch Coarse | −48…+48 |
| 07 | Part Pitch Fine | −50…+50 |
| 08 | Output Assign | MIX, DIR-1/2/3 |
| 09 | Effect Send Mode | DRY, REV, C+R, D+R |
| 0A | Effect Send Level | 0–100 |
| 0B | (dummy) | |

Part 8 ist der Rhythm-Part; die Parameter 08–0A gelten dort nicht.

**Part-Patches 1–7:** `02 0n 00 00`, je 486 Byte im Aufbau des Temp-Patches.

**System mode:** `00 00 00 00` = PATCH / PERFORMANCE / RHYTHM.

## Phasen

| Phase | Inhalt | Ort | Ergebnis |
|---|---|---|---|
| **P0 Prüfen** ✅ | `01` und `02 0n` in beiden Modi lesen; Moduswechsel über `00 00 00 00` hin und zurück; Common-Layout bestätigen; HIL-Sweep für Part-Parameter und `02 00`; Verhalten bei Patchwahl am Part (überschreibt das Gerät `02 0n`?) | nachts am Gerät | Logbuch, Spec `verified` |
| **P1 Kern** ✅ | Spec: Block `performance` (Common + Part × 8) mit Parametern; Patch-Codec mit Basisadresse (Area/Part); Typ `Setup` (Performance + 7 Patches), `.syx` lesen/schreiben (Temp- und Internal-Dumps); FFI | Rust, offline | Tests grün |
| **P2 Verbindung** ✅ | `DeviceLink`: Freigabeliste statt „nur 03"; Lesen von Performance + Part-Patches; Model: Bearbeitungsziel (Patch-Modus / Part n), Gerätemodus lesen und schalten | Host | Test-Haken |
| **P3 Oberfläche** ✅ | Tab „Performance": 7 Kanalzüge (Patchname, an/aus, Kanal, Level, Pan, Tune, Out, FX-Send), Stimmen x/24, gemeinsame Effekte mit Skizzen; Part-Auswahl im Panel-Kopf; Effekt-Hinweise je Part; Vorhören auf Part-Kanal; Moduswahl Patch/Performance sichtbar in der Leiste | Host | Referenzbilder |
| **P4 Bibliothek** | Setups als Einträge (Performance + 7 Patches), Suche, Vergleich; Import der 16 internen Performances aus dem Backup | Host + Kern | |
| **P5 Abnahme** | Setup bauen, auf mehreren Kanälen spielen, speichern, nach Neustart wieder senden | André | |

## Technische Fragen aus P0 — beantwortet 2026-10-03 (Logbuch)

1. RQ1 an `02 0n` im Patch-Modus: **ja**. Part 1 ist dort gleich dem Temp-Patch
   `03`; Parts 2–7 stehen dort, sind aber **nicht beschreibbar** (DT1 wird
   ignoriert). Setups also nur im PERFORMANCE-Modus senden.
2. Patchnummer am Part ändern **lädt den Speicher-Patch** nach `02 0n` (Bank allein
   nicht). Auch der **ganze Performance-Block** lädt die Part-Patches neu. Die
   Reihenfolge „Performance, dann Patches" ist Pflicht und bestätigt.
3. Moduswechsel bestätigt nach **≈ 0,46–0,53 s**. Beim Wechsel nach PERFORMANCE
   lädt das Gerät Performance und Part-Patches aus dem Speicher; `03` ist dann Part 1.
4. Nach dem 144-Byte-Block reichen 150 ms (`common_settle`). Einzelne Parameter mit
   20 ms Abstand: 664 Schreibvorgänge ohne Befund. Ein Patchwechsel braucht
   ≈ 600 ms bis zur nächsten Antwort.

## Stand P1 (2026-10-03)

* **Spec:** `[performance]` mit 28 Common- und 11 Part-Parametern, abgelesen an den
  Handbuchseiten App. 51–52.
  * Lückenlos laut Spec-Prüfung, Dummy-Bytes reserviert.
  * Die Handbuch-Beispiele mit Part 1 bei `0x2E` sind falsch; Tabelle und Backup
    zeigen `0x30`.
* **Kern** (`jd990-core/src/performance.rs`):
  * `Performance` mit get/set, Namen, Diff und Lesen aus Dumps. Frühe 572-Byte-
    Backups werden toleriert.
  * Regel für die Stimmenreservierung: Summe ≤ 24, der Block wird immer ganz
    geschrieben.
  * Patch-Platz-Text (I:/C:/A:/B:).
  * `Setup` (Performance + 7 Patches): Senden in der Reihenfolge Performance →
    Parts, Lesen aus Temp-Dumps, Import aus interner Performance + Backup-Patches
    (Card/Preset → Init, gemeldet).
* **Tests:** Init gültig, Stimmenregel, Roundtrip und Diff (Part 5 Level →
  `01 00 00 64`), Setup-Roundtrip und Reihenfolge, alle 16 Backup-Performances
  lesbar.
* **FFI:**
  * `jd990_perf_*` mit eigenem Parameterindex, damit die Patch-Ansichten
    unberührt bleiben.
  * `jd990_dt1_retarget`: Patch-DT1 auf Part `02 0n` umadressieren, nie auf den
    internen Speicher.
  * `jd990_system_mode_dt1` (nur PATCH/PERFORMANCE), `jd990_rq1_system`,
    `jd990_rq1_performance`, `jd990_patch_from_bytes_at`.
  * Der C-Smoke-Test deckt das ab.

