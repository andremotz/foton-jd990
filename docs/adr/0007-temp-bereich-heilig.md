# ADR-7 — Schreibsemantik: Temp-Bereich ist heilig, Speicher wird geschont

| | |
|---|---|
| **Status** | angenommen; Schreiben in den internen Speicher bis auf Weiteres nicht freigegeben |
| **Datum** | 2026-09-20 |
| **Herkunft** | Planungsphase, ursprünglich in `docs/03-architektur.md` |

**Entscheidung:**
* Jede Bearbeitung geht **ausschließlich** in den Temporary-Bereich.
* Ein Schreiben in den internen Speicher passiert **nur** auf explizite
  Nutzeraktion mit Zielnummer und Bestätigungsdialog.
* Vor dem allerersten Schreibvorgang bietet der Editor einmalig an, die
  kompletten internen Bänke als `.syx`-Backup zu sichern.

**Begründung:** Der interne Speicher eines 30 Jahre alten Geräts ist
unwiederbringlich, wenn er überschrieben wird — und batteriegepuffertes RAM
ist ohnehin ein Ablaufdatum auf Beinen. Ein Editor, der versehentlich
Werkspatches überschreibt, ist ein Werkzeug, dem man nicht mehr traut.

**Nebenprodukt:** „Alle Bänke sichern" ist ein eigenständig wertvolles Feature
(M2) — und liefert gleich das Testkorpus für die Round-Trip-Tests.

---

## Nachtrag 2026-09-28 — Stand der Freigaben

* **DT1 ist ausschließlich an den Temp-Patch (`03 00 00 00` …) freigegeben.**
  Interner Speicher, System- und Performance-Bereich werden nie beschrieben —
  weder vom Editor noch von Test- und Nachtläufen (Festlegung André, M0).
* Das Speichern in den internen Speicher (Zielnummer + Bestätigung) ist
  **nicht umgesetzt und nicht freigegeben**. Es kommt frühestens, wenn der Editor
  „sauber getestet" ist (Entscheidung André, 2026-09-28).
* Das Backup aller internen Bänke existiert (`jd990 backup <verzeichnis>`: System, 64 Patches, 16 Performances; nur RQ1) und
  liegt lokal in `corpus/` (nie im Repository, ADR-23).
* Der Editor ist Owner des Temp-Bereichs: „Laden", „Init" und ein
  Bibliothekspatch gehen bei bestehender Verbindung sofort in den Temp-Patch
  (siehe [ADR-13](0013-editor-besitzt-temp-patch.md)).

---

## Nachtrag 2026-10-03 — Schreibfreigabe für den Performance-Modus (ADR-22)

Freigegeben von André am 2026-10-03:

| Adresse | Bereich | Freigabe |
|---|---|---|
| `03 …` | Temp-Patch (Patch-Modus) | ✅ seit 2026-09-28 |
| `01 …` | Temp-Performance | ✅ neu |
| `02 00 …` bis `02 06 …` | Temp-Patches der Parts 1–7 | ✅ neu |
| `00 00 00 00` | System mode (nur PATCH/PERFORMANCE) | ✅ neu, **nur diese eine Adresse** |
| sonst `00`, `04`–`0B` | System, Rhythm, interner Speicher, Karte | ⛔ unverändert |

Kein Schreiben in den internen Speicher, auch nicht für Performances.
