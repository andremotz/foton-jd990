# ADR-20 — CI auf GitLab

| | |
|---|---|
| **Status** | **ersetzt** durch GitHub Actions (ADR-23, Nachtrag 2026-10-10) |
| **Datum** | 2026-09-28 |
| **Herkunft** | Wunsch André (GitLab-CI) |

## Entscheidung

`.gitlab-ci.yml` definiert drei Jobs:

| Job | Inhalt | Status |
|---|---|---|
| `rust` | Clippy `-D warnings`, alle Tests, C-Smoke-Test der FFI | Pflicht; lokal und in frischem Klon ohne `corpus/` grün |
| `host-linux` | JUCE-Build unter Linux | `allow_failure`, bis ein Runner ihn einmal gebaut hat |
| `host-macos` | Build + UI-Referenzbilder | nur mit macOS-Runner (`MACOS_RUNNER=1`) |

Hardware-Tests (HIL) laufen nie in der CI, sondern nachts am Studio (ADR-6, ADR-11).

## Konsequenzen

* Tests dürfen `corpus/` nicht voraussetzen. Korpusabhängige Tests überspringen
  sich ohne Korpus.
* Der Linux-Host-Build ist ungeprüft: Lokal stand keine Linux-Umgebung zur Verfügung.
