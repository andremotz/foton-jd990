# 05 — Meilensteinplan

Jeder Meilenstein hat ein **Abnahmekriterium**: einen Satz, der wahr oder
falsch ist. Kein „gefühlt fertig".

---

## M0 — Recherche & Verifikation — ✅ 2026-09-28

> Abnahme: 64/64 Patches byteidentisch über den Temp-Bereich, Adresstabelle in
> `spec/addresses.toml`. Hörprobe durch André steht aus. Details: Logbuch (internes Logbuch).
> **Es wird noch kein Produktivcode geschrieben. Wegwerf-Skripte sind erlaubt und erwünscht.**

### Aufgaben
1. **Toolchain prüfen (zuerst!)**
   - Unitor Family Driver v3 auf macOS 27 installieren und verifizieren
     (offiziell: macOS 10.15.7+, Intel und Apple Silicon)
   - Unitor8 in „Audio-MIDI-Setup" sichtbar? Ports korrekt benannt?
   - SysEx-Test in beide Richtungen mit *SysEx Librarian* und *MIDI Monitor*
     (beide Snoize, kostenlos)
   - **Fallback beschaffen**: ein billiges klassenkompatibles USB-MIDI-Interface
     (~30 €). Zweck: bei Problemen die Frage „liegt es an meinem Code oder am
     Interface?" in einer Minute beantworten zu können
2. **MIDI Implementation beschaffen**
   - JD-990 Owner's Manual PDF (Roland-Support), Anhang „MIDI Implementation"
   - Vollständige Adresstabelle abschreiben → `spec/addresses.toml`
3. **Gegen echte Hardware verifizieren**
   - Alle internen Bänke dumpen und als `.syx` sichern → wird zum Testkorpus
     **und** ist gleichzeitig das erste Backup des Geräts
   - Chunk-Größe der Bulk-Dumps ablesen
   - Einzelne DT1-Parameteränderung senden, hörbaren Effekt bestätigen
   - Prüfen: sendet das Gerät bei Frontplatten-Edits? (M0-Frage 3)
   - Prüfen: antwortet es auf Identity Request?
   - Timing austesten: ab welcher Sendedichte verwirft das Gerät Nachrichten?
4. **Strukturen abgleichen**
   - JDTools-`JD-990.hpp` gegen das offizielle Handbuch prüfen
   - Abweichungen dokumentieren (JDTools ist reverse-engineered, das Handbuch
     ist die Primärquelle — aber JDTools ist praxiserprobt)

### Abnahmekriterium
> Ein Wegwerf-Skript holt Patch I:11 aus dem Gerät, schreibt ihn in eine
> `.syx`-Datei, sendet genau diese Bytes zurück in den Temp-Bereich — und das
> Gerät zeigt und klingt exakt wie vorher. Die vollständige Adresstabelle liegt
> als Datei vor.

**Wenn M0 scheitert, scheitert das Projekt — und zwar nach zwei Wochen statt
nach zwei Monaten. Das ist der Zweck dieses Meilensteins.**

---

## M1 — Core-Bibliothek — ✅ 2026-09-28 (eigenes Korpus)

> `jd990 verify` über 64 Werkspatches + Bank-Dump: 0 Abweichungen. Netz-Sammlungen
> fehlen noch als Korpus (Quelle/Lizenz offen).

### Aufgaben
- `spec/params.toml` vollständig für Patch-Ebene (486 Parameter)
- `spec/enums.toml`: 195 Wellenformnamen, Filtermodi, LFO-Wellenformen,
  Effekttypen, Structure Types, Control Destinations
- `jd990-spec`: Spec-Parser + Codegen (`build.rs`)
- `jd990-core`: Patch-Modell, SysEx-Encoder/Decoder, Prüfsumme,
  Adressarithmetik, `.syx`- und SMF-Import
- Round-Trip-Tests gegen das M0-Korpus
- Property-Tests (`proptest`): random valid patch → encode → decode → identisch
- `jd990-cli`: `show`, `verify`, `diff`, `convert`

### Abnahmekriterium
> `jd990 verify corpus/*.syx` läuft über alle 192 Werkspatches plus
> Netz-Sammlungen und meldet **null** Byte-Abweichungen beim Round-Trip.
> Kein MIDI-Interface angeschlossen.

---

## M2 — MIDI-I/O, Hardware-Verifikation, Backup — ✅ 2026-09-28

> `jd990 hil-sweep`: 463 Parameter, 3631 Schreibvorgänge, 0 Befunde (Geräteregeln
> modelliert). `jd990 backup` identisch zum M0-Backup.

### Aufgaben
- `jd990-midi`: Portverwaltung, Device Discovery, Device-ID-Konfiguration
- Rate Limiter und Coalescing-Queue (ADR-8)
- CLI: `dump`, `send`, `set <param> <wert>`, `backup-all`, `monitor`
- **HIL-Sweep** (ADR-6): automatisierter Volltest aller Parameter gegen Hardware
- Vollständiges Bank-Backup mit Fortschrittsanzeige

### Abnahmekriterium
> `jd990 hil-sweep --all` durchläuft jeden Parameter der Spec, setzt Min/Max/
> Mitte/Zufallswerte, liest zurück und meldet **null** Abweichungen — und
> **null** Kollateraländerungen an Nachbarparametern.

**Das ist der Punkt, ab dem das Datenmodell bewiesen ist.**

---

## M3 — Generisches UI  ◀ **aktueller Planungshorizont** — 🔶 in Arbeit

> JUCE-Standalone (seit 2026-09-28: JUCE 9.0.2) läuft am Gerät (lesen, Parameter senden, Patch senden —
> byteexakt geprüft). Abnahme (Patch nur über die Oberfläche bauen) braucht André.

> Bis hierher ist der Weg beschlossen. Ob es nach M3 weitergeht und wie weit,
> wird **nach** M3 entschieden — mit einem laufenden Editor als Grundlage statt
> auf Basis einer Vermutung.

### Aufgaben
- JUCE-Projektgerüst (aktuelles Release), CMake-Build, zunächst nur Standalone-Target
  (Plugin-Formate erst in M6)
- FFI-Layer `jd990-ffi` (C-ABI, Zielabnehmer C++)
- Aus der Spec generierte Oberfläche: Gruppen als Sektionen, Parameter als
  Slider/Dropdown/Schalter mit Anzeigewerten
- Live-Verbindung zum Gerät, Verbindungsstatus sichtbar
- Patch laden/speichern/senden

### Abnahmekriterium
> Ein kompletter Patch lässt sich ausschließlich über die Oberfläche von Grund
> auf bauen, ohne CLI, ohne Handbuch. Optisch anspruchslos, funktional vollständig.

**Hier wird innegehalten und neu bewertet.** Ab M3 existiert ein Werkzeug, das
seinen Zweck bereits erfüllt. Alles Weitere ist Komfort — und darf deshalb auch
entfallen, ohne dass das Projekt gescheitert wäre.

---

# Ausbaustufen

> **Entscheidung André, 2026-09-28:** M3 abrunden, dann **M4 und M5 umsetzen**.
> M6 (Plugin) erst, „wenn alles sauber getestet ist". M7/M8 bleiben offen.
>
> Ursprünglicher Text: Geplant, aber **nicht beschlossen**. Zweck dieser Kapitel ist nicht, sie
> abzuarbeiten, sondern sicherzustellen, dass M1–M3 keine Entscheidung trifft,
> die sie verbaut.

---

## M4 — JD-800-Oberfläche — 🔶 umgesetzt 2026-09-28, Abnahme durch André offen

### Aufgaben
- Panel-Design: Raster, Farbsystem, Fader-/Knopf-Komponenten (vektorbasiert)
- Patch-Ebene, Tone-Ebene A/B/C/D, Control Matrix
- Structure-Typen als Signalflussgrafiken
- Effektketten als sortierbare Blockdiagramme
- 4-Spalten-Tonevergleich, Layer-Link mit relativer Änderung
- Tastaturbedienung, Feinmodus, Undo/Redo

### Abnahmekriterium
> Programmieren am Editor geht schneller und angenehmer als am Gerät —
> nachweisbar an einem konkreten Patch, der auf beide Arten gebaut wird.

---

## M5 — Librarian — 🔶 umgesetzt 2026-09-28 (inkl. JD-800-Import), Abnahme offen

### Aufgaben
- SQLite-Bibliothek: Tags, Notizen, Herkunft, Favoriten, Volltextsuche
- Bankverwaltung, Drag & Drop, Bulk-Import ganzer `.syx`/`.mid`-Sammlungen
- Compare / A-B, Patch-Diff (welche Parameter unterscheiden sich?)
- Init-Patch, Randomize (gewichtet, nicht uniform), Morph zwischen zwei Patches
- **JD-800-Import** mit Konvertierung (Algorithmen: JDTools, BSD-3, Attribution)
- Export: einzelner Patch, Bank, komplettes Gerät

### Abnahmekriterium
> Eine aus dem Netz geladene JD-800-Banksammlung ist importiert, konvertiert,
> durchsuchbar und spielbar.

---

## M6 — Plugin-Targets

### Aufgaben
- Standalone + VST3 + AU + CLAP aus einem Build
- **Zustandspersistenz im DAW-Projekt** (der Hauptgrund für diesen Meilenstein)
- Parameter-Automation ausgewählter Parameter
- MIDI-Learn für Hardware-Controller
- Dokumentation der Windows-Port-Exklusivität (ADR-4)
- Notarisierung macOS / Signierung

### Abnahmekriterium
> Ein DAW-Projekt wird geschlossen und wieder geöffnet; der JD-990 steht danach
> ohne Zutun wieder exakt auf dem gespeicherten Patch.

---

## M7 — Performance, Rhythm, System

### Aufgaben
- Performance-Modus: 7+1 Parts, Part-Parameter, Patch-Zuordnung, Key-/Velocity-Zonen
- Special Setup (Rhythm): 61 Keys mit je vollem Tone — **eigene, listenbasierte
  Ansicht**, kein JD-800-Panel. Key-Auswahl per virtueller Klaviatur
- System Setup
- Erweiterung der Spec (nicht des Codes) um diese Bereiche
- HIL-Sweep auf die neuen Bereiche ausdehnen

### Abnahmekriterium
> Ein kompletter Multitimbral-Setup samt Drumkit ist ausschließlich am Rechner
> gebaut, gespeichert und reproduzierbar geladen.

---

## M8 — Feinschliff (fortlaufend)

Kandidaten, nach Nutzen zu priorisieren:
- Patch-Versionierung / Zeitreise im Edit-Verlauf
- Hörbarer A/B-Vergleich mit Zwischenspeicher
- Presets für Teilbereiche („diesen Filter-Env anderswohin kopieren")
- Bibliotheks-Export als druckbare Patch-Liste
- Generatoren: Morph-Sequenzen, Zufalls-Patches mit musikalischen Constraints
- Tastatur-Shortcut-Schema, Bedienungsanleitung

---

## Abhängigkeitsgraph

```
    ├──────── beschlossen ────────┤├───────── optional ─────────
M0 ──▶ M1 ──▶ M2 ──▶ M3 ──▶ M4 ──▶ M5 ──▶ M8
                      │              │
                      └──────────────┴──▶ M6
                                     │
                                     └──▶ M7
```

M6 (Plugin) und M7 (Performance/Rhythm) sind **voneinander unabhängig** und
können nach Lust und Laune getauscht werden. M4 vor M5 ist Geschmackssache —
wer lieber erst Ordnung in die Patch-Sammlung bringt, dreht das um.
