# ADR-9 — Scope: nur Patch-Modus

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-09-27 |
| **Herkunft** | Entscheidung André (M0-Planung) |

## Kontext

Die Planung (M7, Spec-Vorgabe in F2) sah vor, Performance, Rhythm/Special Setup
und System von Anfang an mit abzudecken. André arbeitet mit dem JD-990
ausschließlich im Patch-Modus.

## Entscheidung

* **Bearbeitet wird nur der Patch-Modus**, also Temp-Patch, Patch Common und Tones A–D.
* **Rhythm/Special Setup ist komplett descoped.**
* **Performances werden nur gelesen und gesichert** (Backup), nie editiert, nie geschrieben.
* **Erweiterungskarten** sind außerhalb des Scopes. Karten-Wellenformen bleiben
  im Datenmodell erhalten und werden angezeigt, sind aber nicht wählbar
  (`locked` in der Spec, Risiko R7).
* **System-Einstellungen** am Gerät (Rx-Kanal, Device ID …) werden nicht vom
  Editor verwaltet.

## Konsequenzen

* M7 schrumpft auf „Performances sichern". Die Spec beschreibt Performance nur,
  soweit das Backup es braucht (144 Byte je Performance, nicht 572 wie zuerst
  angenommen, siehe Logbuch).
* Das JD-800-Panel-Konzept (ADR-4, `04-ui-konzept.md`) deckt den ganzen Scope ab.
  Eigene Listenansichten für Performance/Rhythm entfallen.
* Hinweis: In `06-risiken.md` heißen die Risiken R1–R10 und die offenen Fragen
  F1–F5. Die Anforderungen in [10-anforderungen.md](../10-anforderungen.md)
  tragen deshalb eigene Kürzel (`AN-…`).

---

## Nachtrag 2026-10-03 — Performance-Modus kommt dazu

Performances werden jetzt auch **bearbeitet**, über die Temp-Bereiche und mit
Setups im Editor. Speicher-Schreiben gibt es weiterhin nicht. Rhythm/Part 8
bleibt draußen. Siehe [ADR-22](0022-performance-modus.md).
