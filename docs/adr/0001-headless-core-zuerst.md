# ADR-1 — Headless Core zuerst, UI später

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-09-20 |
| **Herkunft** | Planungsphase, ursprünglich in `docs/03-architektur.md` |

**Entscheidung:** Meilensteine M1/M2 liefern eine Bibliothek plus ein CLI.
Keine grafische Oberfläche vor M3.

**Begründung:** Das gesamte Projektrisiko steckt im Datenmodell, nicht in der
Oberfläche. Ein CLI, das `jd990 dump --patch I:11 > patch.syx`,
`jd990 show patch.syx` und `jd990 set tone.a.tvf.cutoff 90` kann, beweist das
Fundament und ist bereits eigenständig nützlich. Eine GUI, die auf einem
falschen Adressmodell sitzt, beweist nichts.

**Konsequenz:** Man sieht später „etwas Hübsches" als bei einem UI-first-Ansatz.
Das ist bewusst in Kauf genommen.
