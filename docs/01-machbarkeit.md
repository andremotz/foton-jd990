# 01 — Machbarkeit

## Die kurze Antwort

Ja. Das Projekt ist technisch gut beherrschbar, weil es **keine harten
Unbekannten** enthält — nur Fleißarbeit mit hohem Risiko für Flüchtigkeitsfehler.
Genau dagegen lässt sich architektonisch vorgehen.

## Warum das Problem einfacher ist als es aussieht

Ein Hardware-Editor ist im Kern **kein Audio-Projekt**. Es gibt keine DSP,
keine Latenzanforderung im Sub-Millisekundenbereich, keine
Echtzeit-Thread-Akrobatik. Der Kern besteht aus drei Dingen:

1. **Bytes ↔ Datenmodell** (SysEx-Codec, Adressarithmetik, Prüfsumme)
2. **Datenmodell ↔ Oberfläche** (Widgets, Wertebereiche, Anzeigeformate)
3. **Transport** (MIDI-Ports auf, Bytes raus, Bytes rein, Rate Limiting)

Punkt 1 ist exakt spezifiziert und vollständig testbar — ohne Hardware, gegen
echte Patch-Dumps. Punkt 3 ist gelöste Standardtechnik. Der eigentliche Aufwand
liegt in Punkt 2, und der ist linear in der Parameteranzahl — also planbar.

## Warum das Problem schwieriger ist als es aussieht

**Die Menge.** Ein JD-990-Patch hat ca. **486 Parameter-Bytes**
(Herleitung siehe [02-jd990-protokoll.md](02-jd990-protokoll.md)):

```
4 Tones × 92 Bytes            = 368
Patch Common / Key / EQ / …   =  63
Effekt-Sektion (A+B)          =  55
                              -----
                                486
```

Jeder einzelne davon hat: eine Adresse, einen Rohwertebereich, eine
Anzeige-Abbildung (oft nicht-linear, oft mit Offset, oft eine Enum-Tabelle),
ein Label und eine Zugehörigkeit zu einer UI-Gruppe. **Ein einziger
Off-by-one im Adress-Offset verschiebt alle folgenden Parameter** und äußert
sich als „der Editor klingt irgendwie falsch" — ein Bug, den man beim
Draufschauen nicht sieht.

Das Special Setup (Rhythm) verschärft das: 61 Keys × 106 Bytes ≈ **6.500 Bytes**.

**Schlussfolgerung:** Handgeschriebene Parameter-Definitionen sind der sichere
Weg ins Scheitern. Deshalb: deklarative Spezifikation + Codegen + automatisierte
Hardware-in-the-Loop-Verifikation. Siehe [03-architektur.md](03-architektur.md),
ADR-3 und ADR-6.

## Was schon existiert — und warum es nicht reicht

| Lösung | Stärke | Schwäche |
|---|---|---|
| **Sound Diver** (Emagic/Apple) | Der Referenz-Editor seiner Zeit, Unitor8-Integration | Tot. Kein macOS-X-Nachfolger, läuft nicht auf aktuellen Systemen |
| **Patch Base** (Coffeeshopped) | Gepflegt, macOS/iOS, JD-990-Profil vorhanden | Generisches UI-Framework, kein JD-800-Look, Abo-Modell, nicht anpassbar |
| **Ctrlr-Panels** | Kostenlos, existierende JD-990-Panels | Ctrlr selbst ist faktisch unmaintained, Panels oft unvollständig/buggy |
| **JSynthLib / Edisyn** | Open Source, gut getestete Librarian-Logik | Java-Swing-Optik, Fokus Librarian statt Editor |
| **JDTools** (sagamusix) | **Vollständige, verifizierte Datenstrukturen**, BSD-3-Clause | Reines CLI-Konvertierungstool, kein Editor, kein MIDI-I/O |

**Die wichtigste Erkenntnis der Recherche:** JDTools enthält die komplette,
in der Praxis erprobte Feldreihenfolge für `Tone990`, `Patch990` und
`SpecialSetup990`. Das ist unter BSD-3-Clause nutzbar (mit Attribution) und
eliminiert den größten Rechercheblock. Es bleibt zu tun: die **Adressen** aus
der offiziellen MIDI-Implementation ergänzen und gegen echte Hardware verifizieren.

## Aufwandsabschätzung

Annahme: Hobbyprojekt, ca. 6–10 h/Woche, ein Entwickler mit KI-Unterstützung.

| Meilenstein | Umfang | Kalenderzeit (realistisch) |
|---|---|---|
| M0 Recherche & Verifikation | klein, aber zwingend | 1–2 Wochen |
| M1 Core-Bibliothek + CLI | mittel | 3–4 Wochen |
| M2 MIDI-I/O + HIL-Verifikation | mittel | 2–3 Wochen |
| M3 Generisches UI-Skelett | mittel | 3–4 Wochen |
| M4 JD-800-Oberfläche | **groß** (Designarbeit!) | 6–10 Wochen |
| M5 Librarian | mittel | 3–4 Wochen |
| M6 Plugin-Targets | mittel | 2–4 Wochen |
| M7 Performance / Rhythm / System | groß | 4–6 Wochen |
| M8 Feinschliff | offen | fortlaufend |

**Erstes wirklich benutzbares Werkzeug (M0–M3): ca. 2–3 Monate.**
Danach ist alles Weitere Verbesserung an etwas, das bereits funktioniert —
das ist die wichtigste Eigenschaft dieses Zuschnitts.

## Die drei Dinge, die über Erfolg oder Scheitern entscheiden

1. **M0 nicht überspringen.** Erst wenn ein CLI einen Patch aus dem Gerät holt,
   parst, serialisiert und *byteidentisch* zurückschreibt, ist das Fundament
   bewiesen. Alles davor ist Spekulation, alles danach ist Bauen.
2. **Parameter-Spec als Datei, nicht als Code.** Sonst wird der Editor bei
   Parameter #300 unwartbar.
3. **Hardware-in-the-Loop-Sweep.** Ein automatisierter Testlauf, der *jeden*
   Parameter auf Min/Max/Zufallswert setzt und zurückliest, findet
   Adressfehler, die kein Handbuchlesen findet. Der JD-990 steht hier —
   dieser Vorteil muss genutzt werden.
