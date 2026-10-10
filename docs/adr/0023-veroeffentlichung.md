# ADR-23 — Veröffentlichung als Open Source auf GitHub

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-10-10 |
| **Herkunft** | André: „ich will das Tool open source auf GitHub releasen und später ein Video drüber machen" |
| **Ändert** | F3 in `06-risiken.md` (Veröffentlichung war offen), Nachtrag ADR-4 (Lizenzwege) |

## Entscheidung (André, 2026-10-10)

1. **Lizenz: GNU AGPLv3** (`LICENSE`; in den Crates `AGPL-3.0-or-later`). JUCE wird
   unter AGPLv3 genutzt. JDTools (BSD-3) bleibt mit Urheberhinweis in
   `THIRD-PARTY-NOTICES.md`.
2. **README auf Englisch.** Doku und Oberfläche bleiben vorerst deutsch.
3. **Frische Historie auf GitHub:** Der öffentliche Stand beginnt mit einem neuen
   ersten Commit. Die vollständige Entstehungsgeschichte bleibt im privaten
   GitLab-Repo.
4. **Private Arbeitsnotizen** (Logbuch `docs/08-logbuch.md`, Runbook
   `docs/09-nachtbetrieb.md`, Entwicklungsanleitung, ADR-11) werden **nicht
   veröffentlicht**. Sie stehen in
   `.gitignore` und werden lokal weitergeführt. Verweise darauf heißen in der Doku
   „internes Logbuch".
5. **Nie im Repository:** Roland-Werkspatches, Gerätedumps (`corpus/`), Handbuch
   (`reference/`) und Panel-Grafik (F3). Testdaten sind selbst erzeugt
   (`jd990 init`).
6. **Markenrecht:** beschreibender Gebrauch („editor for the Roland JD-990"), Hinweis
   „not affiliated with Roland", kein Logo.

## Konsequenzen

* **CI:** `.github/workflows/ci.yml` enthält Rust (Clippy, Tests, C-Smoke) und einen
  macOS-App-Build. Die GitLab-CI bleibt für das private Repo.
* **Vor oder kurz nach der Veröffentlichung:**
  * ~~Device ID einstellbar machen~~ — erledigt 2026-10-10 („Unit #" in der Tab-Leiste,
    gespeichert; FFI `jd990_set_device_id`);
  * englische Oberfläche als Option;
  * signiertes und notarisiertes macOS-Release (Apple-Entwicklerkonto) oder
    zunächst „nur aus dem Quellcode".
* **Lizenzfolge:** Wer den Editor weitergibt oder verändert verteilt, muss den
  Quellcode unter AGPLv3 mitgeben.

---

## Nachtrag 2026-10-10 — GitHub wird der Hauptort

André: „wir könnten vom privaten GitLab auf das public GitHub als main
umschwenken, sonst wird das Development zu komplex."

* **Entwickelt wird direkt auf `github.com/andremotz/foton-jd990` (`main`).** Das
  private GitLab-Repo bleibt als Archiv der Entstehungsgeschichte; es wird nicht
  mehr fortgeschrieben.
* **Name: FotonJD990**, analog zu Foton Kanban: App, Fenstertitel, README.
  Einstellungen und Bibliothek bleiben im bisherigen Ordner
  `~/Library/Application Support/JD-990 Editor/`, damit nichts verloren geht.
* **Private Arbeitsnotizen** stehen in der lokalen Ausschlussliste
  (`.git/info/exclude`) und nicht in `.gitignore`, damit auch ihre Namen nicht
  öffentlich auftauchen.
* **CI:** nur noch GitHub Actions; `.gitlab-ci.yml` entfällt
  ([ADR-20](0020-ci-gitlab.md) ist damit ersetzt).
* **README-Bilder** werden reproduzierbar erzeugt (`design/screenshot/make.sh`):
  App-Aufnahme in Retina-Auflösung im macOS-Fensterrahmen auf eigenem
  Hintergrund, Demo-Patch `host/tests/fixtures/demo.syx`.

