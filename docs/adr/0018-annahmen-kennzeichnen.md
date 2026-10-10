# ADR-18 — Annahmen kennzeichnen; Hardware und Gehör entscheiden

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-09-28 |
| **Herkunft** | Effekt-Schalter-Fehler (Hörtest André), Erfahrungen aus M0–M2 |

## Kontext

Nicht alles lässt sich aus Handbuch und Rücklesen beweisen. Ein Beispiel ist die
Zuordnung der Effekt-Block-Schalter: Das Gerät speichert „block-1 … block-4 sw",
das Handbuch sagt nicht, ob das Positionen oder Effekte sind. Rücklesen beweist
nur, dass Bytes ankommen, nicht, was sie bewirken.

## Entscheidung

* **Jede unbewiesene Annahme steht an zwei Stellen:** als Kommentar im Code und
  im Logbuch mit „⚠️ Annahme" und dem Test, der sie klären würde.
* **Hardware schlägt Handbuch, Gehör schlägt Rücklesen.** Widerspricht eine
  Beobachtung einer Annahme, wird die Annahme korrigiert und der Befund in Spec,
  Code-Kommentar und Logbuch festgehalten.
* In der Spec markiert `verified` die Prüfung gegen das Handbuch;
  Hardware-Bestätigungen (HIL) stehen im Logbuch.

## Anwendungsfall: Effekt-Schalter

* **Annahme:** Schalter N gehört zu Position N.
* **Befund (Hörtest 2026-09-28):** Nach „Spectrum vor Distortion" war die
  Distortion aus. Der Editor hatte die Schalter mit den Effekten verschoben.
* **Korrektur:** Schalter N gehört fest zum Effekt (A: DS, PH, SP, EN · B: CHO, DLY,
  REV). Umsortieren ändert nur die Sequenz. Die Spec-Labels heißen jetzt „Dist SW" …
* **Noch offen:** JD-800-Konvertierung der Schalter, Layer = TONE SWITCH (am Gerät
  gegenprüfen).
