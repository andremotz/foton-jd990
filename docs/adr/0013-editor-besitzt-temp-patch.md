# ADR-13 — Der Editor besitzt den Temp-Patch; gesendet wird nur der Unterschied

| | |
|---|---|
| **Status** | angenommen, umgesetzt |
| **Datum** | 2026-09-28 |
| **Herkunft** | Abendtest André: „der Init-Button geht nicht" |

## Kontext

„Init" hat den Patch nur im Editor geladen. Am Gerät änderte sich nichts, man
hörte also nicht, was man sah. ADR-8 legt fest, dass der Editor Owner des
Temp-Bereichs ist, das war aber nicht konsequent umgesetzt.

## Entscheidung

* **Was der Editor lädt, geht bei bestehender Verbindung sofort in den
  Temp-Patch des Geräts:** Init, Datei laden, „Laden & senden" aus der Bibliothek.
* **Undo/Redo, Sprünge im Verlauf, A/B, Morph, Variation, Tone kopieren/tauschen
  und Effekte umsortieren senden nur den Unterschied** (`jd990_patch_diff_dt1`),
  nicht den ganzen Patch.
* Der interne Speicher bleibt unberührt (ADR-7).

## Konsequenzen

* Gerät und Editor stimmen nach jeder Aktion überein. Der Sync-Status sagt
  „Gerät = Editor (gesendet …)".
* Ein Undo über einen Fader ist eine einzige Nachricht, kein 486-Byte-Dump.
