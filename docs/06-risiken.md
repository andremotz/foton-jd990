# 06 — Risiken, offene Fragen, Rechtliches

## Risiken

| # | Risiko | Eintritt | Wirkung | Gegenmaßnahme |
|---|---|---|---|---|
| R1 | **Unitor8 läuft nicht auf macOS 27** — *2026-09-27: erledigt — SysEx in beide Richtungen bestätigt* | niedrig | blockierend | Treiber v3 gilt offiziell ab 10.15.7 für Intel *und* Apple Silicon — aber macOS 27 ist neuer als die Treiberpflege. **Erste Aufgabe in M0.** Fallback: klassenkompatibles USB-MIDI-Interface (~30 €) |
| R2 | **Adressfehler in der Parameter-Spec** | hoch | schleichend, schwer zu finden | HIL-Sweep (ADR-6) inkl. Kollateralprüfung. Genau deshalb existiert dieser Test |
| R3 | JD-990 verwirft Nachrichten bei hoher Senderate | mittel | Editor „hängt" gefühlt | Rate Limiter + Coalescing (ADR-8), Grenzen in M0 empirisch ermitteln |
| R4 | MIDI-Implementation im Handbuch unvollständig/fehlerhaft | mittel | Verzögerung | Zwei unabhängige Quellen: Handbuch **und** JDTools-Strukturen. Wo sie sich widersprechen → Hardware fragen |
| R5 | Frontplatten-Edits werden nicht gesendet | mittel | keine echte Bidirektionalität | Ehrlicher Statusindikator + „Neu einlesen"-Button statt stiller Divergenz |
| R6 | **Projekt versandet bei M4** (UI-Design ist der größte Brocken) | hoch | Projektende | M3 liefert bewusst ein *vollständig funktionierendes*, hässliches UI. Ab da ist jeder weitere Schritt optional statt notwendig |
| R7 | Karten-Wellenformen beschädigen Patches | niedrig | Datenverlust | Byte-exakter Durchlauf unbekannter Werte, niemals „auf gültigen Bereich klemmen". Round-Trip-Test deckt das ab |
| R8 | Batteriegepuffertes RAM des JD-990 stirbt | über die Zeit **sicher** | Totalverlust aller Patches | `backup-all` früh in M2. Nebenbei: Batteriewechsel beim 30 Jahre alten Gerät einplanen |
| R9 | Windows: MIDI-Ports exklusiv, Plugin kommt nicht an den Port | niedrig (Mac-Studio) | Plugin unbrauchbar unter Windows | Dokumentieren, multiclient-Treiber empfehlen. Auf macOS (CoreMIDI) kein Thema |
| R10 | Scope Creep Richtung Erweiterungskarten | mittel | Verzögerung | Explizit out of scope. Datenmodell bleibt trotzdem durchlässig (R7) |

## Offene Fragen — Entscheidungsbedarf

### F1 — Zielplattform und Formfaktor — ✅ **entschieden: JUCE**

| Option | Für | Gegen |
|---|---|---|
| **JUCE → Standalone + VST3/AU/CLAP, 3 OS** | Ein Codebase für alles; Patch wird mit dem DAW-Projekt gespeichert; DAW-Automation | Mehr Aufwand als SwiftUI; bei Veröffentlichung AGPLv3 oder JUCE-Lizenz (Starter kostenlos bis 20.000 $ Umsatz); C++ neben Rust |
| **SwiftUI, nur macOS** | Schönste native Integration; schnellste UI-Entwicklung; CoreMIDI direkt | Kein Plugin; kein Windows/Linux; keine DAW-Zustandspersistenz |
| **Beides** (Core ist geteilt) | Maximale Abdeckung | Zwei UIs zu pflegen — realistisch für ein Hobbyprojekt? |

**Entschieden am 2026-09-20: JUCE**, Standalone + VST3 + AU + CLAP.
Ausschlaggebend war die DAW-Zustandspersistenz — wer ein Projekt nach zwei
Jahren öffnet, will nicht raten, welcher Patch im JD-990 stand.
Siehe [ADR-4](adr/0004-host-juce.md).

### F2 — Wie weit soll das Projekt gehen? — ✅ **entschieden: zunächst bis M3**

Beschlossen ist der Weg **M0 bis M3**: Core, MIDI-I/O, HIL-Verifikation und ein
funktionierendes generisches UI. Danach wird neu bewertet.

Das ist der risikoärmste Zuschnitt, den dieses Projekt haben kann. Er endet an
einem Punkt, an dem etwas Benutzbares existiert, statt an einem Punkt, an dem
etwas Halbfertiges liegt — und er enthält genau die Arbeit, die in *jeder*
denkbaren Ausbaustufe gebraucht wird.

**Konsequenz für M1–M3:** M4–M8 sind Optionen, keine Zusagen. Damit sie später
noch offenstehen, kosten zwei Dinge jetzt fast nichts und später viel:

1. Die Parameter-Spec deckt **von Anfang an auch Performance, System und
   Special Setup** ab. Das ist derselbe Arbeitsschritt, nur mit mehr Zeilen —
   nachträglich wäre es eine Strukturänderung.
2. Der Core bleibt **frei von UI- und MIDI-Abhängigkeiten** (ADR-1). Das ist die
   einzige Eigenschaft, die ein späteres Plugin-Target überhaupt möglich macht.

### F3 — Veröffentlichung geplant? — ✅ **entschieden 2026-10-10: Open Source auf GitHub, AGPLv3 (ADR-23)**

Haltung bis auf Weiteres: **veröffentlichungsfähig bleiben, ohne jetzt Aufwand
dafür zu treiben.** Praktisch sind das drei Dinge, die nichts kosten, solange
man sie von Anfang an macht — und teuer werden, wenn man sie nachrüstet:

1. **Keine Original-Panelgrafik verwenden.** Das Panel wird ohnehin
   vektorbasiert neu gezeichnet (siehe [04-ui-konzept.md](04-ui-konzept.md));
   eingescannte Roland-Grafik würde eine spätere Veröffentlichung blockieren.
2. **JDTools-Attribution von Anfang an sauber führen.** Jede aus `JD-990.hpp`
   übernommene Struktur bekommt einen Quellvermerk direkt in der Spec-Datei.
   Nachträglich ist nicht mehr rekonstruierbar, was von wo stammt.
3. **Werkspatches im Korpus als nicht-verteilbar markieren** — `corpus/` in die
   `.gitignore` bzw. in ein separates, privates Repository.

Die eigentliche Lizenzentscheidung fällt erst, wenn veröffentlicht wird. Bis
dahin ist sie folgenlos. Mit JUCE heißt das: AGPLv3 **oder** die kostenlose
JUCE-Starter-Lizenz, die auch eine Weitergabe ohne Quellcode erlaubt (korrigiert
2026-09-29, Einzelheiten im Nachtrag zu [ADR-4](adr/0004-host-juce.md)).

### F4 — Welcher Unitor8 genau? — ✅ **geklärt: Unitor8 MkII (USB)**

*2026-09-27:* Treiber lädt unter macOS 27 auf Apple Silicon, Ports 1–8 in
CoreMIDI sichtbar. Siehe Logbuch (internes Logbuch).

Unitor8 MkII (USB) oder das ursprüngliche Modell? Der Treiber v3 adressiert die
USB-Generation. Relevant für R1.

### F5 — Soll JD-800-Patch-Import dazu?
Erschließt eine große Patch-Szene. Die Konvertierungsalgorithmen existieren
(JDTools). Vorgeschlagen für M5, aber streichbar.

---

## Rechtliches

**Trade Dress / Marken.** „JD-800" und „Roland" sind Marken der Roland
Corporation. Für den privaten Gebrauch ohne Belang. Bei Veröffentlichung:
kein Roland-Logo, keine Original-Panelgrafik, keine Bezeichnung, die
Herstellerzugehörigkeit suggeriert. Die zulässige Formulierung ist
„Editor für den Roland JD-990" (beschreibender Gebrauch), nicht
„Roland JD-990 Editor" als Produktname.

**Fremdcode.**
- *JDTools* (© Johannes Schultz) — BSD-3-Clause. Nutzung und Ableitung erlaubt
  bei Weitergabe des Copyright-Vermerks. Gilt insbesondere für die
  Datenstrukturen und die JD-800↔990-Konvertierung.
- *JUCE 9* — doppelt lizenziert: **AGPLv3 oder JUCE-Lizenz** (korrigiert
  2026-09-28; vorher stand hier fälschlich GPL-3).
  * Private Nutzung: keine Pflichten; die AGPL greift erst bei Weitergabe.
  * Weitergabe unter AGPLv3: Der Quellcode der App muss unter AGPLv3 mitkommen.
  * Weitergabe mit JUCE-Lizenz: Die Stufe „Starter" ist bis 20.000 $ Umsatz
    kostenlos und erlaubt auch geschlossene Weitergabe (ergänzt 2026-09-29;
    vorher stand hier, die AGPLv3 sei bei Veröffentlichung Pflicht). Details im
    Nachtrag zu [ADR-4](adr/0004-host-juce.md).

**Werkspatches.** Die 192 Roland-Werkspatches bleiben im lokalen Testkorpus.
Nicht mit veröffentlichen.

**Empfehlung Lizenz:** Die Wahl hängt davon ab, ob der Quellcode offen sein soll.
* Offen: AGPLv3 für die App, der Rust-Kern gern MIT/Apache-2.0.
* Geschlossen oder frei wählbar: JUCE-Starter-Lizenz (kostenlos bis 20.000 $
  Umsatz), oder ganz ohne JUCE die SwiftUI-Variante (ADR-21).

---

## Nächster Schritt

**Genau eine Sache:** M0, Aufgabe 1 — Unitor Family Driver v3 installieren und
mit SysEx Librarian + MIDI Monitor einen Patch aus dem JD-990 holen.

Solange diese Bytes nicht auf der Festplatte liegen, ist jede weitere Planung
Spekulation. Sobald sie da sind, ist das Projekt zu 80 % entrisikiert.
