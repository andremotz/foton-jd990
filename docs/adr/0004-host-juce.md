# ADR-4 — Host-Layer: JUCE (immer aktuelles Release)

| | |
|---|---|
| **Status** | angenommen, ergänzt 2026-09-28 (JUCE 9) und 2026-09-29 (Lizenz) |
| **Datum** | 2026-09-20 |
| **Herkunft** | Planungsphase, ursprünglich in `docs/03-architektur.md` |

**Entscheidung:** JUCE (jeweils aktuelles Release, Stand 2026-09-28: **9.0.2**),
ein Codebase → Standalone + VST3 + AU + CLAP, macOS/Windows/Linux.

> *Nachtrag 2026-09-28:* Ursprünglich stand hier „JUCE 8". André hat entschieden,
> immer die neueste Version zu nutzen; M3 wurde von 8.0.15 auf 9.0.2 gehoben
> (kompatibel ohne Codeänderung). JUCE wird als Klon unter `host/JUCE` geführt
> (gitignored); die Version steht im Logbuch.

> Die Entscheidung ist gefallen. ADR-1 (Core zuerst) bleibt trotzdem gültig:
> M1/M2 werden weiterhin headless gebaut, damit das Datenmodell unabhängig vom
> Frontend bewiesen wird. Der Unterschied ist, dass ab sofort *für* JUCE geplant
> wird — der FFI-Layer zielt auf C++, nicht mehr auf zwei mögliche Abnehmer.

**Begründung:**
* Eine JD-800-Oberfläche ist in *jedem* Toolkit ein selbstgezeichnetes UI.
  Der Aufwand unterscheidet sich kaum — aber JUCE liefert alle Zielplattformen
  und alle Plugin-Formate aus derselben Quelle.
* Als Plugin wird der Editor erst richtig nützlich: **Patch-Zustand wird mit
  dem DAW-Projekt gespeichert.** Wer ein Projekt nach zwei Jahren öffnet, will
  nicht raten, welcher Patch im JD-990 stand. Das ist der stärkste Einzelgrund
  für die Plugin-Variante.
* DAW-Automation von JD-990-Parametern fällt als Nebenprodukt ab.

**Wichtige Einschränkung — SysEx-Ausgang bei Plugins:**
VST3 hat keinen brauchbaren Weg, SysEx an den Host auszugeben; die meisten
Hosts unterstützen das schlicht nicht. Der etablierte Weg (und der, den
kommerzielle Hardware-Editoren gehen) ist, dass **das Plugin den MIDI-Port
selbst öffnet**, am Host vorbei. Auf macOS ist CoreMIDI mehrklientenfähig —
unproblematisch. Auf Windows sind MIDI-Ports exklusiv; dort braucht es ggf.
einen multiclient-fähigen Treiber. Das ist zu dokumentieren, nicht zu lösen.

**Lizenz:** JUCE (8 und 9) ist doppelt lizenziert: **AGPLv3 oder JUCE-Lizenz**,
nicht GPL-3 wie hier ursprünglich stand (korrigiert 2026-09-28, siehe
`host/JUCE/LICENSE.md`). Genaueres im Nachtrag 2026-09-29 unten.

**Verworfene Alternativen** (dokumentiert, falls die Entscheidung je revidiert wird):

| Option | Wann sinnvoll |
|---|---|
| **SwiftUI** | Wenn der Editor ausschließlich auf dem Studio-Mac laufen soll. Schönste native Integration, schnellste UI-Entwicklung, aber: kein Plugin, kein Windows/Linux |
| **nih-plug** (Rust) | Wenn alles in einer Sprache bleiben soll. Spart FFI, aber die Rust-UI-Toolkits sind für ein skeuomorphes Fader-Panel noch unreif |
| **iPlug2** (C++) | Wie JUCE, aber permissiv lizenziert. Kleinere Community, weniger fertige Bausteine |
| **Tauri / Web-UI** | Schnellste Iteration beim Layout, aber ein Fremdkörper im DAW-Kontext |

---

## Nachtrag 2026-09-29 — Lizenz richtiggestellt

Bisher stand hier (und in `06-risiken.md`), der Editor müsse bei einer
Veröffentlichung unter AGPLv3 stehen. Das war unvollständig. Stand laut
`host/JUCE/LICENSE.md` und juce.com/get-juce (abgerufen 2026-09-29):

* **Private Nutzung:** keine Pflichten. Die AGPL greift erst bei Weitergabe. Die
  JUCE-FAQ sagt ausdrücklich: Wer keine Software mit JUCE außerhalb der eigenen
  Organisation weitergibt, kann innerhalb der AGPLv3 bleiben.
* **Veröffentlichung**, zwei Wege nach Wahl:

| Weg | Folge |
|---|---|
| **AGPLv3** | Quellcode der ganzen App unter AGPLv3 mitliefern bzw. anbieten |
| **JUCE-Lizenz „Starter"** | **kostenlos** bis 20.000 $ Umsatz; Weitergabe auch ohne Quellcode (closed source) |
| JUCE-Lizenz Indie / Pro | bis 300.000 $ Umsatz (40 $/Monat oder 800 $ einmalig) bzw. unbegrenzt (175 $/Monat oder 3.500 $ einmalig) |

* Für ein Hobbyprojekt ohne Einnahmen genügt also die kostenlose Starter-Lizenz.
  Vor einer Veröffentlichung ist das JUCE-9-EULA im Wortlaut zu lesen; geprüft
  wurde hier nur die Übersicht, nicht jede Klausel.
* **Der Rust-Kern** (`crates/`) hängt nicht von JUCE ab. Er kann unabhängig davon
  unter einer eigenen Lizenz stehen, z. B. MIT oder Apache-2.0.
* **Die SwiftUI-Variante** (ADR-21, Branch `pov/swiftui-native`) nutzt JUCE nicht.
  Dort ist die Lizenz frei wählbar.
* Unverändert gilt: JDTools (BSD-3) braucht den Urheberhinweis; Roland-Werkspatches
  und das Handbuch werden nie mitverteilt.

Keine Rechtsberatung. Die JUCE-FAQ verweist bei Einzelfragen selbst auf eigene
rechtliche Prüfung bzw. sales@juce.com.
