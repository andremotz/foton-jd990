# ADR-15 — UI-Tests: deterministische Referenzbilder mit 0 Pixel Toleranz

| | |
|---|---|
| **Status** | angenommen, umgesetzt |
| **Datum** | 2026-09-28 |
| **Herkunft** | Wunsch André (UI-Referenzbilder), Erfahrung mit Mojibake-Fehlern |

## Entscheidung

* `host/tests/ui_snapshots.sh` rendert feste Szenarien über Kommandozeilen-Haken
  (`--offline --testmode --size … --snapshot=…`) und vergleicht sie
  pixelgenau mit `host/tests/golden/` (`imgdiff.swift`).
* **Toleranz 0 Pixel.** Das Rendern ist auf demselben Rechner deterministisch,
  `--testmode` fixiert Uhrzeit und Datum. Lockerbar über `UI_TOLERANCE`.
* **Eigene Fixtures** (`host/tests/fixtures/*.syx`, erzeugt mit `jd990 init`) und
  eine Wegwerf-Bibliothek. Nie Roland-Werkspatches, nie Andrés Bibliothek.
* **Gewollte UI-Änderung:** Bild ansehen, dann `--update`. Das gehört zu jedem
  UI-Commit.
* Keine UI-Automation per Maus (keine Bildschirmrechte nötig, läuft ohne Gerät).

## Konsequenzen

* Die Tests haben sofort einen echten Fehler gefunden („MORPH A â B").
* Die erste Toleranz (0,1 %) ließ eine Fader-Änderung um einen Schritt durch,
  daher 0.
* Nur macOS. Ein anderer Rechner oder eine andere macOS-Version rendert Schrift
  evtl. minimal anders. Die Referenzbilder laufen in der CI deshalb nur im
  macOS-Job (ADR-20).
