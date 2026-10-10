# ADR-12 — Host-Architektur: ein Model als Zustandszentrale

| | |
|---|---|
| **Status** | angenommen, umgesetzt |
| **Datum** | 2026-09-28 (M3/M4) |
| **Herkunft** | Umsetzung M3–M5 |

## Kontext

Mehrere Ansichten zeigen und ändern denselben Patch: Panel, Patch & Effekte,
Alle Parameter, Bibliothek und das LCD. Geräteregeln (Bereich, Key-Range zieht
mit, LSB-„#"-Adressen), Undo, Link-Modus und Sync müssen überall gleich wirken.

## Entscheidung

* **`Model`** (`host/Source/Model.*`) hält Patch, Auswahl, Undo, Link, Sperren und
  Sync-Status. Es ist ein `juce::ChangeBroadcaster` und meldet synchron.
* **Ansichten schreiben nur über `Model::set()`** und lesen über `get()/display()`.
  `set()` läuft in dieser Reihenfolge:
  1. Undo-Schritt,
  2. Link (gelinkte Tones relativ),
  3. Rust-Kern (Bereich, Geräteregeln),
  4. DT1 in die Warteschlange,
  5. Sync-Status und Meldung an die Ansichten.
* **Fachlogik liegt im Rust-Kern** (`jd990-core`) hinter der C-ABI (`jd990-ffi`):
  Codec, Regeln, Variation/Morph, Ähnlichkeit, JD-800-Konvertierung, Bibliothek.
  Der Host enthält nur Darstellung und Bedienung.
* **`DeviceLink`** ist der einzige Weg zum Gerät (JUCE-MIDI). Er schreibt nur in
  den Temp-Bereich und macht Coalescing sowie Token-Bucket (ADR-8).
* **Die Spec erzeugt die Oberfläche mit:** Gruppen, Reihenfolge, Farben und
  Steuerelementtyp kommen aus `params.toml`. Sonderansichten (Hüllkurven,
  Structure, Effektketten, Skizzen) sitzen *zusätzlich* darauf.
* **„Alle Parameter"** (die M3-Ansicht) bleibt dauerhaft als vollständige,
  numerische Ansicht erhalten.
* **Test-Haken** (`--set`, `--undo`, `--libselect` …) rufen dieselben Wege auf
  wie die Bedienung, keine Abkürzungen.

## Konsequenzen

* Eine Regel wird genau einmal implementiert und gilt in jeder Ansicht.
* JUCE-Fallen, die dabei aufgefallen sind:
  * `TextButton::triggerClick` und `TextEditor::setText` melden asynchron.
    Test-Haken rufen die Aktion deshalb direkt auf.
  * Nicht-ASCII-Literale brauchen `String::fromUTF8`.
