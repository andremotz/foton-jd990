# ADR-2 — Sprache des Core: Rust mit C-ABI

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-09-20 |
| **Herkunft** | Planungsphase, ursprünglich in `docs/03-architektur.md` |

**Entscheidung:** `jd990-core` in Rust, exportiert als `staticlib`/`cdylib` mit
C-kompatibler Schnittstelle. Konsumierbar aus Swift **und** aus C++/JUCE.

**Begründung:**
* Bit-Fummelei mit exakten Wertebereichen ist genau die Domäne, in der Rusts
  Typsystem, `#[non_exhaustive]`-Enums und Property-Testing (`proptest`) den
  größten Nutzen bringen.
* `serde` löst Library-Serialisierung nebenbei.
* Der C-ABI-Layer hält **beide** Frontend-Optionen offen — genau die
  Aufschiebbarkeit aus ADR-1.
* Cargo-Tests brauchen keine Build-Infrastruktur; die Testschleife bleibt schnell.

**Verworfene Alternative — C++20:** würde direkte Wiederverwendung des
JDTools-Codes erlauben und FFI sparen. Aber: JDTools wird ohnehin nur als
*Referenz* gelesen (die Strukturen werden in die Spec-Datei überführt, nicht
verlinkt), und der FFI-Layer ist bei einer so schmalen API (~30 Funktionen)
überschaubar.

**Verworfene Alternative — alles in Swift:** verschließt Windows/Linux und
Plugin-Formate endgültig.
