# ADR-17 — Grafisches Bearbeiten und Skizzen

| | |
|---|---|
| **Status** | angenommen, umgesetzt |
| **Datum** | 2026-09-28 |
| **Herkunft** | Wünsche André (Hüllkurven-Zeichnung, Visualisierung im grünen Display, „braucht es dann noch Regler?") |

## Entscheidung

* **Hüllkurven (Pitch/TVF/TVA Env) sind der primäre Editor.**
  * Die Grafik zeigt den aktuellen Tone live und ist beschriftet (L0, T1, L1 … SUS).
  * Punkte werden gezogen: waagerecht = Zeit, senkrecht = Pegel, ⇧ = fein,
    ⌥ = nur eine Richtung. Doppelklick setzt auf Init.
  * Beim Überfahren und Ziehen stehen die Zahlen am Punkt.
* **Die T- und L-Fader sind standardmäßig eingeklappt** und lassen sich je Gruppe
  per „Werte" aufklappen (gespeichert). Velo, T-Velo und T-KF bleiben immer als
  Fader, weil sie keine Kurvenpunkte sind.
* **Skizzen sind Konzeptbilder aus den Parametern, keine Klangmessung:**
  * im Panel-LCD: Hüllkurve, LFO, Filter, Effekte;
  * im Tab „Patch & Effekte": ein grünes Mini-Display je Effekt und für den EQ.
  
  Bandlagen, Kurvenformen und Zeitachsen sind schematisch.
* **Jeder Wert bleibt numerisch erreichbar** („Alle Parameter", ADR-12).

## Konsequenzen

* Die Fader-Wand wird deutlich kürzer; die Grafik ist höher.
* Sichtbare Abweichungen zwischen Skizze und Gehör sind kein Fehler im Codec,
  sondern Grenzen des Konzeptbilds. Das Logbuch nennt das ausdrücklich.
