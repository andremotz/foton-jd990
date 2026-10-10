# ADR-3 — Parameter-Spezifikation als Datei, nicht als Code

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-09-20 |
| **Herkunft** | Planungsphase, ursprünglich in `docs/03-architektur.md` |

**Die wichtigste Entscheidung des Projekts.**

**Entscheidung:** Alle ~486 Patch-Parameter (plus Performance, System, Rhythm)
werden in einer versionierten `params.toml` beschrieben. Aus ihr werden
generiert: Rust-Structs, Getter/Setter mit Bereichsprüfung, SysEx-Offsets,
UI-Metadaten, Anzeigeformatierung und Testfälle.

```toml
[[param]]
id      = "tone.wg.pitch_coarse"
offset  = 0x08              # relativ zum Tone-Basisoffset
label   = "Coarse Tune"
group   = "WG"
raw     = { min = 16, max = 112 }
display = { kind = "linear", min = -48, max = 48, unit = "st" }

[[param]]
id      = "tone.tvf.filter_mode"
offset  = 0x28
label   = "Filter Mode"
group   = "TVF"
raw     = { min = 0, max = 2 }
display = { kind = "enum", table = "filter_mode" }

[enums.filter_mode]
values = ["HPF", "BPF", "LPF"]
```

**Begründung:**
* Ein Parameter existiert an **genau einer** Stelle. Korrektur = eine Zeile.
* Reviewbar: die Spec lässt sich Zeile für Zeile gegen das Handbuch prüfen —
  generierter Code nicht.
* Der Hardware-in-the-Loop-Sweep (ADR-6) iteriert direkt über die Spec.
* Das generische UI (M3) entsteht dadurch fast geschenkt.
* Nicht-lineare Anzeigen (Delay-Zeiten, LFO-Rate, EQ-Frequenzen) werden als
  Tabellen in der Spec gehalten, statt als Sonderfälle im Code zu verrotten.

**Konsequenz:** Ein Codegen-Schritt im Build (`build.rs`). Der Aufwand
amortisiert sich spätestens bei Parameter #50.
