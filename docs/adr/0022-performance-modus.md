# ADR-22 — Performance-Modus bearbeiten: Temp-Bereiche, Setups im Editor

| | |
|---|---|
| **Status** | angenommen (Planung); Umsetzung in Phasen P0–P5, siehe [11-performance-plan.md](../11-performance-plan.md) |
| **Datum** | 2026-10-03 |
| **Herkunft** | André: „der Editor läuft so gut, dass ich überlege, doch den Perform-Mode zu implementieren, um mehrere solcher Patches gleichzeitig programmieren und spielen zu können" |
| **Ändert** | [ADR-9](0009-scope-patch-modus.md) (Performances waren „nur sichern"), [ADR-7](0007-temp-bereich-heilig.md) (Schreibfreigabe) |

## Kontext

Laut MIDI-Implementation (App. 50–52) hält der JD-990 im Performance-Modus:

| Bereich | Adresse | Inhalt |
|---|---|---|
| Temp-Performance | `01 00 00 00` | 144 Byte: Common 48 + 8 Parts × 12 |
| Temp-Patches der Parts 1–7 | `02 0n 00 00` (n = 0…6) | je 486 Byte, **gleicher Aufbau** wie der Temp-Patch `03` |
| Part 8 | — | Rhythm-Part, verweist auf ein Rhythm Setup |

Daraus folgt: Der vorhandene Patch-Editor kann jeden Part bearbeiten. Es ändert
sich nur die Basisadresse.

Einschränkungen des Geräts (Handbuch Pfom-8 … Pfom-10):

* **Effekte:** Die Patch-Effekte gelten im Performance-Modus nicht. Ausnahme:
  Part 1 behält seine Gruppe A. Alle Parts teilen sich Chorus, Delay und Reverb der
  Performance und haben je Part einen Send-Modus (DRY/REV/C+R/D+R) mit Pegel.
* **Stimmen:** 24 insgesamt, mit Reservierung je Part. Ein 4-Tone-Patch braucht
  4 Stimmen je Note.
* **Speicher:** Ein Part verweist auf einen Patch im Speicher. Wählt man am Gerät
  eine andere Performance oder Patchnummer, lädt das Gerät den Patch neu, und
  Änderungen im Temp-Bereich gehen verloren.

## Entscheidung (André, 2026-10-03)

1. **Schreibfreigabe** erweitert um die Temp-Bereiche `01` (Performance) und
   `02 00…06` (Part-Patches 1–7).
2. **Moduswechsel aus der Oberfläche:** Der Editor schaltet Patch- und
   Performance-Modus selbst, damit klar ist, was gerade passiert. Dafür ist genau
   **eine** Adresse im System-Bereich freigegeben: `00 00 00 00` (System mode:
   PATCH/PERFORMANCE; RHYTHM wird nie gesetzt). Jeder andere Schreibzugriff auf
   `00` bleibt verboten.
3. **Kein Schreiben in den internen Speicher** (`05` Performance, `06` Patch, Karte).
   Ein Setup lebt im Editor: Performance + 7 Patches als eine Einheit in der
   Bibliothek bzw. als `.syx`. Beim Laden sendet der Editor alles in die
   Temp-Bereiche.
4. **Eigene MIDI-Kanäle je Part** sind der Normalfall (Sequencer/DAW, Zonen am
   Keyboard). Layern über gleiche Kanäle bleibt möglich, ist aber nicht der
   Ausgangspunkt der Oberfläche.
5. **Part 8 (Rhythm)** und Rhythm Setups bleiben außerhalb des Scopes (ADR-9).

## Konsequenzen

* **Kern:**
  * Die Spec bekommt einen Performance-Block (Common + Part) mit Parametern statt
    nur einer Blockgröße.
  * Die Patch-Funktionen bekommen eine wählbare Basisadresse.
  * Neuer Datentyp **Setup** (Performance + 7 Patches), mit `.syx` als
    kanonischem Format (ADR-5).
* **Host:** Das Model hat ein Bearbeitungsziel (Patch-Modus oder Part 1–7).
  `DeviceLink` prüft die Adressen gegen eine Freigabeliste statt gegen `03`.
* **Oberfläche:**
  * Performance-Ansicht mit 7 Kanalzügen und Stimmen-Summe x/24.
  * Part-Auswahl wie Tone A–D.
  * Patch-Effekte werden bei Parts 2–7 als „im Performance-Modus ohne Wirkung"
    gekennzeichnet, bei Part 1 nur Gruppe B.
* **Risiko:** Gerätewechsel der Performance oder Patchnummer überschreibt
  Part-Patches. Der Editor muss das erkennen (Abgleich) und anbieten, sein Setup
  neu zu senden.
* **Erst prüfen (P0, nachts):** Lesen von `01` und `02 0n` in beiden Modi,
  Aufbau, Verhalten beim Moduswechsel, HIL über beide Bereiche.
