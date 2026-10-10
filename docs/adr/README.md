# Architekturentscheidungen (ADRs)

Eine Datei je Entscheidung. Jede Datei beginnt mit Status, Datum und Herkunft.
Neue Entscheidungen bekommen die nächste freie Nummer; überholte werden nicht
gelöscht, sondern als „ersetzt durch ADR-n" markiert. Wer eine Entscheidung
ändert, ergänzt einen datierten **Nachtrag** in der Datei und einen Eintrag im
Logbuch (internes Logbuch).

Die Anforderungen, auf die sich die Entscheidungen beziehen, stehen in
[10-anforderungen.md](../10-anforderungen.md). Das Schichtenmodell und der
Modulschnitt stehen in [03-architektur.md](../03-architektur.md).

| ADR | Entscheidung | Status |
|---|---|---|
| [1](0001-headless-core-zuerst.md) | Headless Core zuerst, UI später | angenommen |
| [2](0002-rust-mit-c-abi.md) | Core in Rust mit C-ABI | angenommen |
| [3](0003-spec-als-datei.md) ⭐ | Parameter-Spec als Datei (`params.toml`), Codegen | angenommen |
| [4](0004-host-juce.md) | Host: JUCE, immer aktuelles Release (9.0.2); AGPLv3 oder JUCE-Lizenz (Starter kostenlos) | angenommen |
| [5](0005-syx-und-sqlite.md) | `.syx` kanonisch, SQLite als Bibliothek | angenommen |
| [6](0006-roundtrip-und-hil.md) ⭐ | Round-Trip- und Hardware-in-the-Loop-Tests | angenommen, umgesetzt |
| [7](0007-temp-bereich-heilig.md) | Nur Temp-Bereiche beschreiben (`03`, seit 2026-10-03 auch `01`, `02 00–06`, System mode); interner Speicher nie | angenommen |
| [8](0008-sync-und-rate-limit.md) | Warteschlange, Rate Limit, ehrlicher Sync-Status | angenommen; Abgleich-Intervall offen |
| [9](0009-scope-patch-modus.md) | Scope: Patch-Modus; Rhythm descoped (Performances: siehe ADR-22) | angenommen, ergänzt |
| [10](0010-geraeteverbindung.md) | Fester Port 2, automatisches Verbinden, eine Instanz | angenommen |
| 11 | Zeitfenster und Regeln für automatisierte Hardware-Tests (interne Notiz) | angenommen |
| [12](0012-host-architektur-model.md) | Host: ein Model als Zustandszentrale, Logik im Rust-Kern | umgesetzt |
| [13](0013-editor-besitzt-temp-patch.md) | Editor besitzt den Temp-Patch; nur Unterschiede senden | umgesetzt |
| [14](0014-undo-modell.md) | Undo per Schnappschuss, gebündelt, mit Verlauf | umgesetzt |
| [15](0015-ui-tests-referenzbilder.md) | UI-Referenzbilder mit 0 Pixel Toleranz | umgesetzt |
| [16](0016-zoom.md) | Zoom wie im Browser, Standard 130 % | umgesetzt |
| [17](0017-grafisches-bearbeiten.md) | Hüllkurven als primärer Editor, Skizzen als Konzeptbilder | umgesetzt |
| [18](0018-annahmen-kennzeichnen.md) | Annahmen kennzeichnen; Hardware und Gehör entscheiden | angenommen |
| [19](0019-bibliothek-aehnlichkeit-und-sperren.md) | Ähnlichkeitsmaß, Variation/Morph, Gruppen-Sperren | umgesetzt |
| [20](0020-ci-gitlab.md) | GitLab-CI (Rust Pflicht, Host optional) | ersetzt (GitHub Actions, ADR-23) |
| [22](0022-performance-modus.md) | Performance-Modus: Temp-Bereiche, Moduswechsel aus der UI, Setups im Editor | angenommen (Planung) |
| [23](0023-veroeffentlichung.md) | Open Source auf GitHub (FotonJD990): AGPLv3, englische README, frische Historie, GitHub als Hauptort, Arbeitsnotizen privat | angenommen |
