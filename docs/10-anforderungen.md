# 10 — Anforderungen

Was der Editor können muss, woher die Anforderung kommt und wie weit sie
erfüllt ist. Die Entscheidungen dahinter stehen in den [ADRs](adr/README.md),
der Verlauf im Logbuch (internes Logbuch), die Reihenfolge im
[Meilensteinplan](05-meilensteine.md).

**Kürzel:**

| Kürzel | Bereich |
|---|---|
| `AN-G` | Gerät & Kommunikation |
| `AN-P` | Panel / Tone-Ebene |
| `AN-E` | Patch & Effekte |
| `AN-B` | Bibliothek |
| `AN-F` | Performance-Modus (ab 2026-10-03, [ADR-22](adr/0022-performance-modus.md)) |
| `AN-U` | Bedienbarkeit |
| `AN-S` | Sicherheit & Betrieb |
| `AN-Q` | Qualität & Nachweis |

Die Kürzel sind bewusst anders als die Risiken `R1–R10` und die offenen Fragen
`F1–F5` in [06-risiken.md](06-risiken.md).

**Status:**

| Zeichen | Bedeutung |
|---|---|
| ✅ | umgesetzt und am Gerät oder von André bestätigt |
| 🔶 | umgesetzt, Test am Gerät / Abnahme durch André offen |
| ⏳ | offen |
| ⛔ | bewusst nicht vorgesehen |

**Quelle** nennt, wer die Anforderung gestellt hat: A = André, P = Planung
(Dokumente 01–06), mit Datum.

---

## Gerät & Kommunikation

| ID | Anforderung | Quelle | Status | Nachweis / Umsetzung |
|---|---|---|---|---|
| AN-G1 | Patch-Parameter (Temp-Patch, Common + Tones A–D) byteexakt lesen und schreiben | P, M0–M2 | ✅ | HIL-Sweep 463/463; erschöpfend Tone A (6696 Rohwerte) und Patch Common (5974), 0 Befunde (Logbuch) |
| AN-G2 | Jede Änderung im Editor ist sofort am Gerät hörbar | P (ADR-8) | ✅ | `Model::set` → DT1; Abendtest 2026-09-28 |
| AN-G3 | Laden, Init und Bibliothekspatch gehen bei Verbindung sofort in den Temp-Patch | A 2026-09-28 („Init geht nicht") | ✅ | [ADR-13](adr/0013-editor-besitzt-temp-patch.md); André: „init geht jetzt" |
| AN-G4 | Nur Unterschiede senden (Undo, Verlauf, A/B, Morph, Variation, Kopieren) | P (ADR-8) | 🔶 | `jd990_patch_diff_dt1`; offline getestet |
| AN-G5 | Senderate begrenzen, Fader-Schwenks zusammenfassen, Gerätetiming einhalten | P (ADR-8), M0-Messung | ✅ | Token-Bucket 60 %, Coalescing, `common_settle` 150 ms |
| AN-G6 | Ehrlicher Sync-Status: sieht man, ob Editor und Gerät übereinstimmen | P (UI-Prinzip 7) | ✅ | Statusleiste rechts; „Vom Gerät lesen" |
| AN-G7 | Frontplatten-Änderungen am Gerät erkennen | P (R5) | 🔶 | Gerät sendet nichts (M0). Stattdessen Abgleich alle 5 s. ⏳ Intervall/Verhalten: Entscheidung André offen (ADR-8 Nachtrag) |
| AN-G8 | Nach Programmwechsel den Temp-Patch neu lesen | P (ADR-8) | ⏳ | C3 laut Handbuch stumm; nicht gezielt getestet |
| AN-G9 | Automatisch mit Port 2 verbinden, bei fehlendem Interface weiter suchen | A 2026-09-28 | ✅ | [ADR-10](adr/0010-geraeteverbindung.md) |
| AN-G10 | Patch vorhören: Taste gedrückt halten = Note/Akkord auf Kanal 12 | A 2026-09-28 | ✅ | „▶ Hören"; Noten werden beim Beenden sicher ausgeschaltet |
| AN-G11 | Alle internen Patches automatisch einlesen und im Editor verwalten | A 2026-09-28 | 🔶 | Bibliothek → „Vom Gerät (I:11–88)", nur RQ1; live noch nicht benutzt |
| AN-G12 | Komplettes Backup (System, 64 Patches, 16 Performances) als `.syx` | P (ADR-7, R8) | ✅ | `jd990 backup <verzeichnis>`, nur RQ1 → `corpus/` (lokal, nie im Repo) |

## Panel / Tone-Ebene

| ID | Anforderung | Quelle | Status | Nachweis / Umsetzung |
|---|---|---|---|---|
| AN-P1 | Jeder Patch-Parameter über die Oberfläche einstellbar, ohne Handbuch (M3-Abnahme) | P (M3) | 🔶 | Alle 190 Parameter in „Alle Parameter"; Abnahme durch André offen |
| AN-P2 | Panel im JD-800-Geist: Gruppen in Rolands Gliederung, Sektionsfarben, Fader mit Anzeigewert | P (UI-Konzept) | 🔶 | M4 umgesetzt, Abnahme offen |
| AN-P3 | Tone A–D wählen; Link: Änderung wirkt relativ auf gelinkte Tones | P (UI-Prinzip 2) | 🔶 | Link-Knopf, ⇧-Klick wählt Tones |
| AN-P4 | 4×-Vergleich: eine Gruppe, A–D nebeneinander, Abweichungen hervorgehoben | P (UI-Prinzip 1) | 🔶 | „4× Vergleich" |
| AN-P5 | Tone ganz oder gruppenweise kopieren, Tones tauschen | A 2026-09-28 | 🔶 | Menü „Tone kopieren / tauschen…", ein Undo-Schritt |
| AN-P6 | Hüllkurven als beschriftete Grafik (T1, L1 …) | A 2026-09-28 | ✅ | [ADR-17](adr/0017-grafisches-bearbeiten.md) |
| AN-P7 | Hüllkurven direkt ziehen; Zahlen am Punkt; eine Richtung sperren | A 2026-09-28 | ✅ | André: „funktionieren super"; ⌥-Sperre offline getestet |
| AN-P8 | T-/L-Fader bei Bedarf zuschaltbar, sonst nur Grafik | A 2026-09-28 | 🔶 | Knopf „Werte" je Gruppe, gespeichert |
| AN-P9 | Visualisierung im grünen Display je nach berührtem Parameter (Hüllkurve, LFO, Filter, Effekt) | A 2026-09-28 | 🔶 | `drawLcdGraph` |
| AN-P10 | Wellenformen schnell durchblättern (▲▼, Mausrad) | A 2026-09-28 | 🔶 | Gesperrte Karten-Wellen werden übersprungen |
| AN-P11 | Tone an/aus (Layer) und Edit-Auswahl (Active) als Schalter A–D statt Masken-Liste | A 2026-09-28 | 🔶 | ⏳ Gegenprüfung am Gerät: Layer = TONE SWITCH |
| AN-P12 | Gewichtete Zufallsvariante des Patches (Variation) | P (M5) | 🔶 | Stärke 5–50 %; Wellenform, Schalter, Bereiche, Pegel bleiben |
| AN-P13 | Gruppen für Variation und Morph sperren | A 2026-09-28 | 🔶 | Schloss im Gruppenkopf ([ADR-19](adr/0019-bibliothek-aehnlichkeit-und-sperren.md)) |

## Patch & Effekte

| ID | Anforderung | Quelle | Status | Nachweis / Umsetzung |
|---|---|---|---|---|
| AN-E1 | Structure A&B / C&D als anklickbare Signalflussgrafik | P (UI-Prinzip 5) | 🔶 | |
| AN-E2 | Key-Range und Velocity je Tone mit Tastaturstreifen; Geräteregel „low/high ziehen einander mit" | P, M2-Befund | ✅ | Regel im Rust-Kern, HIL-bestätigt |
| AN-E3 | Effektketten A/B als Blöcke, per Drag & Drop umsortierbar, ON/OFF je Effekt | P (UI-Prinzip 4) | 🔶 | Schalter gehören zum Effekt ([ADR-18](adr/0018-annahmen-kennzeichnen.md)); ⏳ Hörtest nach Fix |
| AN-E4 | Skizze je Effekt und für den EQ | A 2026-09-28 | 🔶 | `FxSketch`, Konzeptbilder |

## Bibliothek

| ID | Anforderung | Quelle | Status | Nachweis / Umsetzung |
|---|---|---|---|---|
| AN-B1 | Patches sammeln, Import `.syx`/`.mid` (Einzel und Bank), Dubletten erkennen | P (M5, ADR-5) | 🔶 | Inhalts-Hash; Drag & Drop |
| AN-B2 | Suche über Name, Quelle, Tags, Notizen; Favoriten | P (M5) | 🔶 | |
| AN-B3 | Export als Einzelpatch oder Bank (`.syx`) | P (ADR-5) | 🔶 | |
| AN-B4 | Vergleich Eintrag ↔ Editor und zweier Einträge miteinander | P (M5), A 2026-09-28 | 🔶 | ⌘-Klick auf zweiten Eintrag |
| AN-B5 | A/B-Umschalten und Morph zwischen Editor und Eintrag, live am Gerät | P (M5) | 🔶 | |
| AN-B6 | Ähnlich klingende Patches finden | A 2026-09-28 | 🔶 | Gewichtetes Maß ([ADR-19](adr/0019-bibliothek-aehnlichkeit-und-sperren.md)), mit echter 64er-Sicherung geprüft |
| AN-B7 | JD-800-Patches importieren und konvertieren | P (F5, M5) | 🔶 | nach JDTools (BSD-3); ⏳ Effekt-Schalter der Konvertierung prüfen |
| AN-B8 | Druckbare Patch-Liste | P (M8) | ⛔ | von André abgelehnt (2026-09-28) |

## Bedienbarkeit

| ID | Anforderung | Quelle | Status | Nachweis / Umsetzung |
|---|---|---|---|---|
| AN-U1 | Undo/Redo über alles, auch Patch-Wechsel; ⌘Z / ⇧⌘Z | P (UI-Konzept) | ✅ | [ADR-14](adr/0014-undo-modell.md) |
| AN-U2 | Verlauf aller Änderungen mit Sprung zu einem früheren Stand | A 2026-09-28 | 🔶 | Knopf „Verlauf" |
| AN-U3 | Größere Schrift / skalierbare Oberfläche, Standard 130 % | A 2026-09-28 | 🔶 | [ADR-16](adr/0016-zoom.md) |
| AN-U4 | Fader per Tastatur (↑↓ ±1, ⇧ ±10), Feinmodus, Doppelklick = Init, Mausrad | P (UI-Konzept) | 🔶 | |
| AN-U5 | Patch laden/speichern als Datei | P (M3) | ✅ | |
| AN-U6 | App-Icon | A 2026-09-28 | ✅ | `design/icon/` |
| AN-U7 | Automatisch vorhören beim Blättern; Wellenform-Browser mit Kategorien | Vorschlag | ⛔ | von André abgelehnt (2026-09-28) |

## Sicherheit & Betrieb

| ID | Anforderung | Quelle | Status | Nachweis / Umsetzung |
|---|---|---|---|---|
| AN-S1 | DT1 ausschließlich an den Temp-Patch; interner Speicher, System, Performance nie beschreiben | A (M0) | ✅ | `DeviceLink`, Runbook; [ADR-7](adr/0007-temp-bereich-heilig.md) |
| AN-S2 | Speichern in den internen Speicher nur mit Zielnummer + Bestätigung | P (ADR-7) | ⏳ | bewusst zurückgestellt, nicht freigegeben |
| AN-S3 | Automatisierte Hardware-Tests nur nachts (23:00–06:30), nur Port 2 | A 2026-09-27 | ✅ | ADR-11 |
| AN-S4 | Werkspatches, Gerätedumps und Handbuch nie im Repository | P (F3) | ✅ | `.gitignore` (`corpus/`, `reference/`) |
| AN-S5 | Keine Original-Roland-Grafik; Attribution für JDTools/JUCE | P (F3) | ✅ | `THIRD-PARTY-NOTICES.md` |
| AN-S6 | Jede Änderung wird committet und gepusht; Logbuch wird fortgeschrieben | A 2026-09-28 | ✅ | je Feature ein Commit auf `m3-m5`, Fast-Forward nach `main` |

## Qualität & Nachweis

| ID | Anforderung | Quelle | Status | Nachweis / Umsetzung |
|---|---|---|---|---|
| AN-Q1 | Parameter-Spec als einzige Quelle; gegen Handbuch geprüft | P (ADR-3) | ✅ | `spec/params.toml` v2, 190 Parameter, `verified` |
| AN-Q2 | Round-Trip- und Hardware-in-the-Loop-Tests | P (ADR-6) | ✅ | `cargo test`, `jd990 hil-sweep` |
| AN-Q3 | UI-Referenzbilder, pixelgenau | A 2026-09-28 | ✅ | 11 Szenarien, [ADR-15](adr/0015-ui-tests-referenzbilder.md) |
| AN-Q4 | CI (seit 2026-10-10 GitHub Actions) | A 2026-09-28 | 🔶 | `.github/workflows/ci.yml`; erster Lauf nach der Veröffentlichung |
| AN-Q5 | Immer das aktuelle JUCE-Release | A 2026-09-28 | ✅ | JUCE 9.0.2 ([ADR-4](adr/0004-host-juce.md)) |

## Performance-Modus

Geplant am 2026-10-03, Phasen in [11-performance-plan.md](11-performance-plan.md).

| ID | Anforderung | Quelle | Status | Nachweis / Umsetzung |
|---|---|---|---|---|
| AN-F1 | Mehrere Patches gleichzeitig programmieren und spielen: Performance mit Parts 1–7 | A 2026-10-03 | 🔶 | P0–P3 umgesetzt, Abnahme André offen |
| AN-F2 | Jeden Part-Patch mit dem vollen Patch-Editor bearbeiten (Panel, Hüllkurven, Effekte, Undo) | A 2026-10-03 | ✅ | `jd990_dt1_retarget`; am Gerät: Part 3 Cutoff → `02 02` (2026-10-03); HIL Part 3 99/0 |
| AN-F3 | Performance-Parameter je Part: an/aus, MIDI-Kanal, Patchwahl, Level, Pan, Tune, Ausgang, FX-Send; Common: Name, Stimmenreservierung, Sync-Part, gemeinsame Effekte | P (Handbuch App. 51–52) | 🔶 | Tab „Performance"; perf-sweep 91/0; am Gerät Part 3 Level 77 ✓ |
| AN-F4 | Moduswechsel Patch ↔ Performance aus der Oberfläche, sichtbar, was gerade aktiv ist | A 2026-10-03 | ✅ | Umschalter in der Tab-Leiste; am Gerät hin und zurück (≈ 0,5 s) |
| AN-F5 | Eigene MIDI-Kanäle je Part als Standard; Vorhören auf dem Kanal des Parts | A 2026-10-03 | 🔶 | Init: Kanal 1–7; ▶ je Kanalzug und „Hören" nutzen den Part-Kanal |
| AN-F6 | Setups (Performance + 7 Patches) im Editor speichern, laden, an das Gerät senden — **nie** in den internen Speicher | A 2026-10-03 | 🔶 | `.syx` laden/speichern + „Setup senden" (byte-gleich am Gerät); Bibliothek für Setups: P4 offen |
| AN-F7 | Ehrliche Anzeige der Geräte-Einschränkungen: Patch-Effekte ohne Wirkung (Parts 2–7, Part 1 nur Gruppe A), Stimmen-Summe x/24 | P (Handbuch Pfom-10) | 🔶 | Hinweise in „Patch & Effekte" und im Tab Performance; Stimmenregel im Kern |
| AN-F8 | Erkennen, wenn das Gerät Part-Patches neu geladen hat (Performance-/Patchwechsel am Gerät), und anbieten, das Setup neu zu senden | P (ADR-22) | 🔶 | Abgleich vergleicht im Performance-Modus die Performance (⚠ bei Abweichung); Part-Patches noch nicht |
| AN-F9 | Interne Performances (Backup) als Setups importieren | P | ⏳ | 16 Performances in `corpus/` |

## Außerhalb des Scopes

| ID | Was | Quelle | Status |
|---|---|---|---|
| AN-X1 | Rhythm / Special Setup bearbeiten | A 2026-09-27 | ⛔ ([ADR-9](adr/0009-scope-patch-modus.md)) |
| AN-X2 | Performances in den internen Speicher schreiben | A 2026-09-27, bestätigt 2026-10-03 | ⛔ (Bearbeiten im Temp-Bereich: siehe AN-F*) |
| AN-X3 | Erweiterungskarten | P (R10) | ⛔ |
| AN-X4 | Plugin-Targets (VST3/AU/CLAP), DAW-Zustand, MIDI-Learn (M6) | A 2026-09-28: „für die Zukunft, wenn alles sauber getestet ist" | ⏳ später |

---

## Offene Punkte (Stand 2026-09-29)

1. **Hörtest Effekt-Schalter** nach dem Fix (AN-E3): Spectrum vor Distortion
   schieben, dann muss die Distortion weiter klingen.
2. **Layer = TONE SWITCH** am Gerät gegenprüfen (AN-P11).
3. **Abgleich-Intervall** entscheiden (AN-G7): einstellbar, standardmäßig aus,
   oder nur im Hintergrund.
4. **„Vom Gerät (I:11–88)"** einmal live benutzen (AN-G11).
5. **Programmwechsel (C3)** testen (AN-G8).
6. **Abnahmen M3–M5** durch André (AN-P1, AN-P2, AN-B*).
7. **GitLab-Pipeline** ansehen (AN-Q4).
