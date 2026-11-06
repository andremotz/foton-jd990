# Design

## Context

- Heute: eine Standalone-App, ein `Model` mit globalen Einstellungen, ein `DeviceLink`,
  der den Port öffnet (ADR-10: fester Port, automatisches Verbinden, eine Instanz).
- Performance-Modus (ADR-22, P0-Befunde): Volle Performance-Blöcke laden die Part-Patches
  neu → Reihenfolge immer Performance, dann Patches. Patchwahl eines Parts lädt ~600 ms
  aus dem Speicher. Im Patch-Modus ignoriert das Gerät DT1 an Part 2–7.
- DAWs laden Plugins teils in **getrennten Prozessen** (Bitwig „pro Plugin", Ableton/Logic
  meist gemeinsam). Ein In-Prozess-Singleton reicht deshalb nicht in jedem Fall.
- JUCE 9.0.2 erzeugt VST3, AU, Standalone (und LV2/AAX), aber **kein natives CLAP**;
  dafür gibt es `clap-juce-extensions` (MIT). VST3-SDK ist seit 3.8 MIT-lizenziert.
- VST3 kann keine SysEx-Nachrichten an die DAW ausgeben, und Rückantworten des Geräts
  kommen über die DAW gar nicht an. Das Plugin muss den Port also selbst (bzw. über
  den Hub) ansprechen — so wie die App heute.

## Goals / Non-Goals

**Goals:**
- Ein Part = eine Spur = eine Instanz; der Klang ist im Projekt gespeichert.
- Beim Öffnen eines Projekts klingt das Gerät wie beim Speichern.
- Die Kanalfrage ist sichtbar gelöst: Jede Instanz sagt, welchen Kanal ihre Spur
  senden muss, und erkennt, wenn es nicht passt.
- Gleicher Editor-Umfang pro Part wie in der App (Panel, Hüllkurven, Undo).

**Non-Goals:**
- Plugin spielt Noten selbst aus; Automation; Bibliotheksverwaltung im Plugin
  (Bibliothek bleibt in der App; im Plugin nur „Patch laden aus Bibliothek").

## Decisions

### 1. Formate
VST3 + AU in Stufe 1, CLAP in Stufe 2 über `clap-juce-extensions` (Bitwig und Reaper
profitieren am meisten). Typ: **Instrument** mit MIDI-Eingang und stiller Stereo-Ausgabe,
weil nur Instrumente in allen DAWs auf einer Instrumentspur sitzen und Noten sehen.

### 2. Kanäle: DAW routet, Plugin zeigt und prüft
- Der Empfangskanal gehört zum Part in der Temp-Performance (Parameter `rx_channel`),
  nicht zum Plugin. Die Instanz zeigt ihn groß an: „Part 3 · Kanal 3".
- Die Instanz sieht die Noten ihrer Spur (MIDI-Eingang des Plugins) und meldet
  „Spur sendet auf Kanal 5, Part 3 hört auf Kanal 3" — die häufigste Fehlerquelle.
- Aktion „Parts auf Kanal 1–7 legen" schreibt `rx_channel` aller Parts in die
  Temp-Performance (erlaubter Bereich 01). Kanal 16 (Perform Ctrl) wird gemieden.
- In Bitwig: Instrumentspur mit FotonJD990 + Gerät „HW Instrument"/„Note Out" auf
  Port 2 und den angezeigten Kanal. Kurzanleitung je DAW im README.
- Verworfen für Stufe 1: Notendurchleitung (Timing/Latenzausgleich der DAW umgangen).

### 3. Ein Port-Besitzer: der Hub
Alle Instanzen senden über **einen** Besitzer des Ports. Er hält die Warteschlange
(ADR-8), die Freigabeliste (ADR-7/22), die Lesezugriffe und den Gerätezustand
(Modus, Performance, welche Instanz welches Ziel besitzt).

Zwei Kandidaten, Entscheidung im Spike (Aufgabe 1):
- **A — Hub-Prozess:** `FotonJD990 Hub`, kopfloser Hilfsprozess im Plugin-Bundle,
  vom ersten Client gestartet, beendet sich nach dem letzten. IPC über
  `juce::InterprocessConnection` (lokaler Socket). Funktioniert bei getrennten
  Plugin-Prozessen und auch mit laufender Standalone-App (die dann selbst Client wird).
- **B — In-Prozess-Singleton** mit Fallback „Port belegt": einfacher, bricht aber bei
  Bitwigs Prozess-Isolation.
Vorentscheidung: **A**, weil B in der wichtigsten Ziel-DAW (Bitwig) nicht trägt.

### 4. Zustand im Projekt
- Part-Instanz: Ziel + 486 Byte Part-Patch + Undo-Verlauf (nur Sitzung) + Sperren.
- Performance-Instanz (höchstens eine pro Projekt): 48 Byte Common inkl. Effekte und
  Part-Bänke/-Nummern als Quelle; ohne sie bleibt die Performance des Geräts stehen.
- **Recall-Reihenfolge:** Der Hub sammelt die Zustände der Instanzen beim Laden,
  wartet 1 s nach der letzten Anmeldung, schaltet bei Bedarf in den Performance-Modus,
  sendet die Performance, dann die Part-Patches (P0-Regel). Danach ist das Gerät
  „Besitz des Projekts"; der Status zeigt „Projekt-Setup gesendet".
- Format im Projekt: versionierter Block (`FJD9` + Version + `.syx`-Bytes), damit
  spätere Versionen alte Projekte lesen.

### 5. Doppelte Ziele
Der Hub vergibt Ziele exklusiv. Eine zweite Instanz am selben Ziel zeigt
„Part 3 wird schon von einer anderen Spur bearbeitet" mit „Übernehmen"; nach der
Übernahme wird die erste schreibgeschützt.

### 6. Code-Schnitt
- `Model` wird instanzfähig: Einstellungen über eine übergebene Quelle (App: Datei,
  Plugin: Projektzustand + gemeinsame Einstellungen), keine statischen Singletons.
- `DeviceLink` bekommt eine Schnittstelle `DeviceChannel` (senden, anfordern,
  Status); Implementierungen `LocalDeviceLink` (App ohne Hub) und `HubClient`.
- `MainComponent` wird im `AudioProcessorEditor` wiederverwendet; Tabs, die im Plugin
  keinen Sinn ergeben (Bibliothek, Performance-Übersicht außerhalb der Performance-Instanz),
  werden ausgeblendet.
- Rust-Kern bleibt unverändert; die Gerätelogik bleibt im Kern (ADR-12).

## Risks / Trade-offs

- **Prozess-Isolation / Sandbox:** AU-Plugins laufen in Logic ggf. in einem
  Sandbox-Prozess (AUHostingService); ob IPC und Prozessstart dort erlaubt sind, klärt
  der Spike. Fallback: Hub = laufende Standalone-App, die der Nutzer startet.
- **Recall überschreibt Gerätezustand** beim Öffnen eines Projekts: gewollt, aber
  sichtbar machen (Statusmeldung; Option „beim Laden nicht senden").
- **Windows:** ohne Windows MIDI Services sperrt der Hub den Port für die DAW → Noten
  kämen nicht mehr an. Unter Windows ist Windows MIDI Services Voraussetzung.
- **Zwei Projekte gleichzeitig offen** (z. B. zwei DAWs): Der Hub behandelt sie wie
  zwei Clients; das letzte Recall gewinnt. Dokumentieren, nicht lösen.
- **Aufwand:** Der Model-Umbau berührt fast jede Datei unter `host/Source`; die
  UI-Referenzbilder (ADR-15) sichern ab, dass die App gleich bleibt.
