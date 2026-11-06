# Proposal

## Why

Im Performance-Modus spielt der JD-990 bis zu sieben Parts auf eigenen MIDI-Kanälen.
In der DAW gehört jeder Part zu einer eigenen Spur. Heute liegt der Editor außerhalb
der DAW: Klänge sind nicht im Projekt gespeichert, und beim Öffnen eines Projekts
klingt das Gerät, wie es gerade eingestellt ist. Als Plugin auf der Spur wäre der
Klang eines Parts Teil des Projekts und direkt neben den Noten bearbeitbar.

## What Changes

- FotonJD990 zusätzlich als **Plugin**: VST3 (macOS, Windows) und AU (macOS);
  CLAP als zweite Stufe. Die Standalone-App bleibt.
- Jede Plugin-Instanz wird an ein **Ziel** gebunden: *Patch-Modus* (Temp-Patch),
  *Part 1–7* oder *Performance gemeinsam* (Name, Stimmenreserve, Chorus/Delay/Reverb).
- **Kanäle:** Die DAW routet die Noten wie bisher selbst zum Gerät (Entscheidung
  André, 2026-11-06). Die Instanz zeigt, auf welchem Kanal ihr Part empfängt, warnt,
  wenn die Spur auf einem anderen Kanal sendet, und bietet „Parts auf Kanal 1–7
  legen" an (schreibt nur die Temp-Performance).
- **Projekt-Recall:** Jede Instanz speichert ihren Part-Patch (bzw. die gemeinsamen
  Performance-Daten) im DAW-Projekt. Beim Öffnen wird das ganze Setup in der
  richtigen Reihenfolge gesendet (Performance zuerst, dann Part-Patches — P0).
- **Ein Besitzer des MIDI-Ports:** Alle Instanzen sprechen über einen gemeinsamen
  Hub mit dem Gerät, damit SysEx nicht durcheinander gerät und Lesezugriffe nicht
  kollidieren. Wie der Hub aussieht, entscheidet ein Spike (Aufgabengruppe 1).
- Zwei Instanzen am selben Ziel: Die zweite zeigt einen Hinweis und bearbeitet erst
  nach „Übernehmen".

**Nicht dabei:** Notendurchleitung durch das Plugin; DAW-Automation von Parametern
(spätere Stufe, Rate Limit ADR-8 klären); AAX (Pro Tools: Avid-Konto, PACE-Signatur);
LV2/Linux; AUv3/iPad; VST2 (abgekündigt, keine Lizenz mehr); Audio-Rückführung.

**Branch:** `feature/plugin-mode`. Merge nach `main` erst nach der Abnahme
(Aufgabengruppe 7) in Bitwig und einer zweiten DAW.

## Capabilities

### New Capabilities
- `daw-plugin`: FotonJD990 als Plugin auf einer DAW-Spur — Zielbindung, Kanalanzeige,
  Speichern im Projekt, Zusammenspiel mehrerer Instanzen mit einem Gerät.

### Modified Capabilities
- (keine; Performance-Modus und Freigabeliste gelten unverändert, siehe ADR-7/22)

## Impact

- `host/CMakeLists.txt`: zweites Ziel `juce_add_plugin` (VST3, AU, Standalone teilt den Code)
- `host/Source`: `Model` pro Instanz statt globaler Zustände (Einstellungen, Undo, Settings),
  `DeviceLink` hinter einer Schnittstelle (lokal oder über den Hub), neuer `PluginProcessor`/`PluginEditor`
- Neuer Hub (Prozess oder geteilte Instanz, siehe design.md)
- ADR-10 („eine Instanz") wird ergänzt: eine Instanz *pro Port-Besitz*, viele Plugin-Instanzen als Clients
- Release-Workflow (`add-release-builds`): Plugin-Artefakte ergänzen, sobald beide Changes in `main` sind
