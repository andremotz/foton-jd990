# Tasks

Branch: `feature/plugin-mode`

## 1. Spike: Wer besitzt den Port? (Entscheidung vor dem Umbau)

- [ ] 1.1 Minimal-Plugin (VST3/AU) mit `juce_add_plugin`, sendet SysEx direkt an Port 2
- [ ] 1.2 Bitwig, Plugin-Hosting „einzeln": zwei Instanzen in getrennten Prozessen senden gleichzeitig lange SysEx — kommen sie unvermischt an?
- [ ] 1.3 Hub-Prototyp: Hilfsprozess im Bundle starten, `InterprocessConnection`, Ping/Senden
- [ ] 1.4 Logic (AU, Sandbox-Prozess): darf das Plugin den Hub starten und verbinden?
- [ ] 1.5 Ergebnis als ADR-25 (Port-Besitz im Plugin-Modus), design.md Entscheidung 3 aktualisieren

## 2. Model und Gerät instanzfähig machen (App bleibt gleich)

- [ ] 2.1 Statische Zustände in `Model` auflösen (Einstellungen, Settings-Datei, Undo)
- [ ] 2.2 Schnittstelle `DeviceChannel`; `DeviceLink` wird `LocalDeviceLink`
- [ ] 2.3 UI-Referenzbilder unverändert (13 Szenarien, 0 px)

## 3. Hub

- [ ] 3.1 Hub-Prozess mit Warteschlange, Rate Limit, Freigabeliste, Leseanfragen (Code aus `DeviceLink`)
- [ ] 3.2 Protokoll: anmelden (Ziel), senden, lesen, Gerätestatus, Ziel-Besitz, abmelden
- [ ] 3.3 `HubClient` als zweite `DeviceChannel`-Implementierung
- [ ] 3.4 Start bei erstem Client, Ende nach letztem; Absturz eines Clients räumt dessen Ziel frei
- [ ] 3.5 Standalone-App als Client, wenn ein Hub läuft

## 4. Plugin-Ziel und Editor

- [ ] 4.1 `PluginProcessor` (Instrument, MIDI-Eingang, stille Ausgabe) und `PluginEditor` mit `MainComponent`
- [ ] 4.2 Zielwahl (Patch-Modus / Part 1–7 / Performance gemeinsam), Ziel-Besitz mit „Übernehmen"
- [ ] 4.3 Tabs je Ziel ein-/ausblenden; Zoom im Plugin-Fenster
- [ ] 4.4 Kanalanzeige, Prüfung des Spurkanals, „Parts auf Kanal 1–7 legen"

## 5. Projekt-Recall

- [ ] 5.1 `getStateInformation`/`setStateInformation` mit versioniertem Block
- [ ] 5.2 Hub sammelt Zustände, sendet nach 1 s Ruhe: Modus → Performance → Part-Patches
- [ ] 5.3 Option „beim Laden nicht senden", Status „Projekt-Setup gesendet" / „nicht abgeglichen"

## 6. Builds und Doku

- [ ] 6.1 CI baut VST3 + AU (macOS) und VST3 (Windows); `pluginval` in der CI
- [ ] 6.2 Release-Workflow um Plugin-Artefakte erweitern (nach Merge von `add-release-builds`)
- [ ] 6.3 README: Plugin-Installation, Kurzanleitung Bitwig/Ableton/Logic für Kanäle und Hardware-Ausgang
- [ ] 6.4 Stufe 2 vormerken: CLAP über `clap-juce-extensions`, Automation (eigener Change)

## 7. Abnahme vor dem Merge (mit JD-990)

- [ ] 7.1 Bitwig: Performance-Instanz + 3 Part-Instanzen, alle hörbar auf eigenen Kanälen
- [ ] 7.2 Projekt speichern, Gerät verstellen, Projekt öffnen → klingt wie gespeichert
- [ ] 7.3 Falscher Spurkanal wird angezeigt; „Parts auf Kanal 1–7 legen" wirkt
- [ ] 7.4 Zweite DAW (Logic AU oder Ableton VST3): Laden, Bearbeiten, Recall
- [ ] 7.5 Standalone-App unverändert (UI-Referenzbilder, Patch-Modus und Performance-Modus)
- [ ] 7.6 Interner Speicher unverändert (Backup vorher/nachher vergleichen)
- [ ] 7.7 `openspec validate add-plugin-mode`, Merge nach `main`, Change archivieren
