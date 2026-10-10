# ADR-5 — `.syx` als kanonisches Dateiformat, SQLite als Bibliothek

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-09-20 |
| **Herkunft** | Planungsphase, ursprünglich in `docs/03-architektur.md` |

**Entscheidung:**
* **Austauschformat: rohes `.syx`.** Bit-exakt, kompatibel mit SysEx Librarian,
  JDTools, Ctrlr, alten Bank-Sammlungen aus dem Netz.
* **Import zusätzlich: `.mid`/SMF** — historisch das gängigste Verteilformat
  für Bank-Dumps (SysEx in einem Standard-MIDI-File verpackt).
* **Bibliothek: SQLite** — für Tags, Suche, Versionen, Herkunft, Notizen.
  Jederzeit als `.syx` exportierbar.

**Begründung:** Ein Editor, der Patches in ein Eigenformat sperrt, ist wertlos,
sobald man ihn nicht mehr pflegt. Die eigene Datenbank ist Komfort; `.syx`
bleibt die Wahrheit.

**Erwägenswert:** JD-800-`.syx` importieren und konvertieren — die Algorithmen
dafür stehen in JDTools (`Convert800to990.cpp`). Damit wird die große
JD-800-Patchszene erschlossen. Vorgemerkt für M5.
