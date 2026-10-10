# ADR-19 — Ähnlichkeit, Variation und Sperren

| | |
|---|---|
| **Status** | angenommen, umgesetzt |
| **Datum** | 2026-09-28 |
| **Herkunft** | Wünsche André (Bibliothek „wirklich hilfreich", Sperren, Verlauf) |

## Entscheidung

**Ähnlichkeit** (`jd990_core::morph::similarity`, 0…1) ist eine gewichtete
Parameterdistanz, keine Audioanalyse:

* Es zählen nur klingende Tones (Layer).
* Die Tones werden über alle 24 Permutationen bestmöglich einander zugeordnet.
* Gewichte: Wellenform 8, Cutoff/Coarse 3, Resonanz/Mode/Level 2, Hüllkurven und
  Pitch-LFO 1, der Rest 0,3.
* Patch-Parameter (Effekte, Key FX) zählen zu 15 %. Active und der Name zählen nicht.

**Variation und Morph** liegen im Rust-Kern. Wellenform, Tone-Schalter,
Structure, Bereiche und Pegel lässt die Variation grundsätzlich unverändert.

**Sperren** gelten je Spec-Gruppe (Schloss im Gruppenkopf) für alle vier Tones.
Umgesetzt im Host (`Model::applyLocks`): Nach Variation bzw. Morph werden
gesperrte Gruppen aus dem Ausgangspatch zurückgesetzt. Sie gelten nur für die
laufende Sitzung.

## Konsequenzen

* Die Prozentwerte sind eine Rangfolge-Hilfe, keine Messgröße. Die erste,
  ungewichtete Fassung lieferte 81–95 % für fast alles und war unbrauchbar.
* Sperren auf Parameterebene sind möglich, aber nicht gebaut (Gruppe reicht bisher).
