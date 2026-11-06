# ADR-24 — Vorhaben mit OpenSpec planen

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-11-06 |
| **Herkunft** | André: „definieren wir schon die Workitems, bevor wir was implementieren?" |
| **Ergänzt** | ADR-README (Ablauf), `docs/10-anforderungen.md` |

## Entscheidung

1. Größere Vorhaben werden vor der Umsetzung als **OpenSpec-Change** unter
   `openspec/changes/<name>/` geplant: `proposal.md` (warum, was, was nicht, Branch),
   `design.md` (Entscheidungen und Alternativen), `specs/` (prüfbares Verhalten mit
   Szenarien) und `tasks.md` (Workitems zum Abhaken).
2. Jeder Change wird auf einem **eigenen Branch** umgesetzt und erst nach der
   Abnahme-Aufgabengruppe in `tasks.md` nach `main` gemergt. Danach wird er archiviert
   (`openspec archive`), seine Specs wandern nach `openspec/specs/`.
3. **ADRs bleiben** für Grundsatzentscheidungen. Was sich in einem Change als
   dauerhafte Entscheidung erweist, wird beim Archivieren als ADR festgehalten.
4. Artefakte auf Deutsch, OpenSpec-Schlüsselwörter (SHALL/MUST, Überschriften) auf
   Englisch. Von OpenSpec erzeugte Anleitungen für KI-Werkzeuge kommen nicht ins Repo.

## Erste Changes

* `add-release-builds` — macOS-App und Windows-`.exe` über GitHub Actions
  (Branch `ci/release-builds`).
* `add-plugin-mode` — FotonJD990 als VST3/AU-Plugin, eine Instanz pro Part
  (Branch `feature/plugin-mode`).

## Konsequenzen

* Kleine Korrekturen brauchen keinen Change.
* `openspec validate` gehört vor jeden Merge eines Changes.
