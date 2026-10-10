# ADR-14 — Undo: Schnappschüsse, gebündelt, mit Verlauf

| | |
|---|---|
| **Status** | angenommen, umgesetzt |
| **Datum** | 2026-09-28 |
| **Herkunft** | UI-Konzept („Rückgängig über alles"), Wunsch André (Verlauf) |

## Entscheidung

* **Jeder Undo-Schritt ist eine vollständige Patch-Kopie** (486 Byte), höchstens 200.
* **Bündelung:**
  * Änderungen am selben Parameter und Tone mit < 800 ms Abstand sind ein Schritt
    (ein Fader-Zug, nicht 100).
  * Eine Mausgeste (`beginGesture/endGesture`, z. B. Hüllkurvenpunkt ziehen) ist
    genau ein Schritt, auch wenn sie zwei Parameter ändert.
  * Ein Klick ohne Bewegung erzeugt keinen Schritt.
* **Verlauf:** Jeder Schritt merkt sich Parameter und Tone oder eine Beschreibung
  („Variation 50 %", „Geladen: …"). Die Liste zeigt „Tone A · TVF · Cutoff: 35 → 62".
  Ein Klick springt über beliebig viele Schritte, gesendet wird dabei nur der
  Unterschied zum Ziel (ADR-13).
* **Laden eines Patches ist rückgängig machbar.** A/B-Umschalten und die
  Morph-Vorschau erzeugen dagegen keinen Schritt.

## Verworfene Alternative

**Befehlsbasiertes Undo** (inverse Operation je Änderung). Die Geräteregeln
verändern Nachbarwerte: Key-Range low/high ziehen einander mit, und das Gerät
klemmt Werte. Inverse Befehle wären dadurch fehleranfällig. Schnappschüsse sind
bei 486 Byte trivial billig und immer korrekt.
