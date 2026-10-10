# 04 — UI-Konzept: JD-800-Geist auf JD-990-Substanz

## Die zentrale Spannung

Der JD-800 hat 58 Fader. Der JD-990 hat **mehr Parameter als der JD-800** —
und kein einziges Bedienelement dafür. Genau diese Lücke soll der Editor
schließen.

Aber: Eine 1:1-Kopie des JD-800-Panels **funktioniert nicht**, weil der JD-990
Dinge hat, die der JD-800 nicht kennt:

| JD-990-exklusiv | Wo im JD-800-Layout? |
|---|---|
| Structure Types A/B und C/D (Ringmod, Sync, Bypass …) | existiert nicht |
| FXM (Frequency Cross Modulation) je Tone | existiert nicht |
| Tone Delay (Mode + Time) je Tone | existiert nicht |
| Pan + Pan KeyFollow je Tone | existiert nicht |
| Velocity Ranges/Points/Fades je Tone | existiert nicht |
| Frei sequenzierbare Effektketten (Group A/B) | JD-800: feste Kette |
| 3-Tap-Delay (Center/Left/Right) | JD-800: einfaches Delay |
| Performance-Modus mit 7+1 Parts | existiert nicht |
| Special Setup / Rhythm (61 Keys) | existiert nicht |

**Designhaltung:** Der JD-800-Look ist die *Leitidee*, nicht das Pflichtenheft.
Was auf dem JD-800 einen Fader hat, bekommt einen Fader an der gewohnten Stelle.
Was nur der JD-990 kann, bekommt eigene, klar abgesetzte Bereiche — sichtbar
als „das ist das Plus des 990", nicht als Fremdkörper.

## Layout-Entwurf

```
┌────────────────────────────────────────────────────────────────────────────┐
│  PATCH  [ Wolkentraum          ]  I:24   ◀ ▶   [Write] [Compare] [Init]     │
├────────────────────────────────────────────────────────────────────────────┤
│  TONE   ⬛A  ⬜B  ⬜C  ⬜D     Layer ▣▣□□   Active ▣▣▣□   Struct A/B: [3]   │
├─────────────────────────────────────────────────────┬──────────────────────┤
│                                                     │  STRUCTURE           │
│   WG          PITCH ENV      TVF        TVF ENV     │   ┌──┐ ┌──┐          │
│   ▮ ▮ ▮       ▮ ▮ ▮ ▮ ▮      ▮ ▮ ▮      ▮ ▮ ▮ ▮ ▮   │   │A │→│B │  Typ 3   │
│   ▮ ▮ ▮       ▮ ▮ ▮ ▮ ▮      ▮ ▮        ▮ ▮ ▮ ▮ ▮   │   └──┘ └──┘          │
│   Wave ▼                                            │  ──────────────────  │
│   ───────────────────────────────────────────────   │  990 EXTRAS          │
│   TVA         TVA ENV        LFO 1      LFO 2       │   FXM   ▮ ▮          │
│   ▮ ▮ ▮       ▮ ▮ ▮ ▮ ▮      ▮ ▮ ▮ ▮    ▮ ▮ ▮ ▮     │   Delay ▮ ▮          │
│   ▮ ▮ ▮       ▮ ▮ ▮ ▮ ▮      ▮ ▮ ▮ ▮ ▮  ▮ ▮ ▮ ▮ ▮   │   Pan   ▮ ▮          │
│                                                     │   Vel   ▮ ▮ ▮        │
├─────────────────────────────────────────────────────┴──────────────────────┤
│  CONTROL MATRIX   Src1 ▼ → [Dest ▼ ▮][Dest ▼ ▮][Dest ▼ ▮][Dest ▼ ▮]        │
│                   Src2 ▼ → [Dest ▼ ▮][Dest ▼ ▮][Dest ▼ ▮][Dest ▼ ▮]        │
├────────────────────────────────────────────────────────────────────────────┤
│  EQ ▮▮▮▮▮▮▮ │ GROUP A  [Dist]→[Phas]→[Spec]→[Enh]  (sequenzierbar, Drag)   │
│             │ GROUP B  [Chor]→[Dly]→[Rev]          Balance ▮               │
└────────────────────────────────────────────────────────────────────────────┘
```

## Gestaltungsprinzipien

### 1. Alle vier Tones gleichzeitig sichtbar machen — auf Wunsch
Der JD-800 zeigt immer nur den selektierten Tone. Das ist eine
Hardware-Beschränkung, keine Designqualität. Der Editor bietet zusätzlich einen
**4-Spalten-Vergleichsmodus**, in dem dieselbe Parameterzeile für A/B/C/D
nebeneinander steht. Beim Bau geschichteter JD-Patches ist das der eigentliche
Gewinn gegenüber der Hardware.

### 2. Layer-Link
Ein Umschalter „Edit: A / B / C / D / **alle gelinkten**". Bewegt man einen
Fader bei aktivem Link, ändern sich alle gelinkten Tones **relativ** (Delta,
nicht absolut) — der übliche Arbeitsschritt „mach alle vier etwas heller".

### 3. Fader mit echtem Wertfeld
Jeder Fader hat unter sich ein editierbares Zahlenfeld mit der **Anzeige** des
Werts (`-12 st`, `LPF`, `420 ms`), nicht dem Rohwert. Der Rohwert ist über
einen Debug-Modus erreichbar — für M0/M1-Verifikation unverzichtbar.

### 4. Der Effektbereich ist ein Flussdiagramm, keine Fader-Reihe
Group A und B sind beim JD-990 **frei sequenzierbar**. Eine Fader-Reihe kann
das nicht abbilden. Deshalb: Blöcke, die per Drag & Drop umsortiert und per
Klick ein-/ausgeschaltet werden; der selektierte Block klappt seine Parameter
darunter auf.

### 5. Structure visuell, nicht numerisch
„Structure Type 4" ist bedeutungslos, solange man nicht das Diagramm aus dem
Handbuch daneben legt. Die sechs Typen werden als **kleine Signalflussgrafiken**
gezeigt, zwischen denen man klickt.

### 6. Gruppierung folgt Roland, Reihenfolge nicht
Die Aufteilung in WG / Pitch Env / TVF / TVF Env / TVA / TVA Env / Tone Common /
LFO / Control ist **Rolands eigene**: So gliedert die MIDI-Implementation den
Tone-Block, und so beschriftet der JD-800 sein Panel. Diese Gliederung wird
übernommen, weil jede Dokumentation und jedes Gespräch über die Kiste sie benutzt.

Die *Reihenfolge* ist dagegen eine reine UI-Entscheidung und steht als
`group_order` in der Spec. Die Byte-Offsets sortieren nach Speicherlayout — das
ist Sache des Codecs und keine sinnvolle Lesereihenfolge für einen Klangbauer.

**Achtung bei „Common":** Es gibt zwei davon. *Tone Common* (Velocity Curve, Hold)
gehört zum einzelnen Tone und existiert viermal pro Patch. *Patch Common* (Level,
Pan, Bend, Portamento) gilt für den ganzen Patch. Im Panel heißen sie deshalb
`TONE COMMON` und `BASICS` — das Verwechslungsrisiko ist real und wäre teuer.

### 7. Sichtbarer Verbindungsstatus
Oben rechts permanent: Port, Device ID, Zeitpunkt des letzten Dumps und ob die
Editor-Sicht dem Gerät entspricht. Ein Hardware-Editor, bei dem man nicht weiß,
ob er gerade synchron ist, ist schlimmer als keiner.

### 8. Kein Skeuomorphismus um seiner selbst willen
Die Erkennbarkeit des JD-800 kommt aus **Layout und Farbe** (dunkles Panel,
die charakteristischen Gruppenfarben, die Fader-Rasterung), nicht aus
fotorealistischen Schrauben und Schlagschatten. Vektorbasiert und skalierbar —
das Panel muss auch auf einem 27"-Display und in einem Plugin-Fenster von
1100×700 funktionieren.

## Was in M4 gebaut wird und was später

| Phase | Umfang |
|---|---|
| **M3 (generisch)** | Aus der Spec generierte Tabellen-/Gruppenansicht. Hässlich, aber vollständig und sofort nutzbar |
| **M4 (JD-800-Look)** | Patch-Ebene + Tone-Ebene + Effekte als gestaltetes Panel |
| **M5** | Librarian, Compare/A-B, Suche |
| **M7** | Performance-Modus (7+1 Parts) und Special Setup (61 Keys) — beides eigene Ansichten, kein JD-800-Panel |

**Wichtig:** M3 ist kein Wegwerfprodukt. Die generische Ansicht bleibt dauerhaft
als „Expertenmodus / alle Parameter" erhalten — sie ist die einzige Ansicht, die
garantiert **vollständig** ist, und sie ist das Werkzeug zur Fehlersuche, wenn
ein Parameter im schönen Panel nicht tut, was er soll.

## Zugänglichkeit und Bedienbarkeit

* Jeder Fader per Tastatur erreichbar; Pfeiltasten = ±1, Shift = ±10
* Feinmodus beim Ziehen (Modifier gedrückt)
* Doppelklick = Initialwert
* Rückgängig/Wiederholen über **alles**, auch über Patch-Wechsel hinweg
* Mausrad über einem Fader ändert den Wert (mit Coalescing, siehe ADR-8)
