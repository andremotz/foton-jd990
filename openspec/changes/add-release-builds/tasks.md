# Tasks

Branch: `ci/release-builds`

## 1. Windows-Build lauffähig machen

- [ ] 1.1 CI-Job `windows-app` (`windows-latest`, MSVC, JUCE 9.0.2) in `ci.yml`, zunächst nur bauen
- [ ] 1.2 Rust-Staticlib unter MSVC linken (Systembibliotheken, `/MT`-Runtime passend zur Rust-Toolchain)
- [ ] 1.3 Kompilierfehler/Warnungen unter MSVC beheben (`-D warnings`-Gleichstand mit macOS)
- [x] 1.4 Einstellungs- und Bibliotheksordner unter `%APPDATA%\JD-990 Editor\` prüfen
- [x] 1.5 Portauswahl/automatisches Verbinden (ADR-10) mit Windows-Portnamen; Meldung, wenn der Port von einem anderen Programm belegt ist
- [ ] 1.6 `cargo test` und C-Smoke-Test auch auf Windows in der CI

## 2. macOS-Release-Artefakt

- [ ] 2.1 Universal Binary: Rust-Kern für `aarch64-apple-darwin` und `x86_64-apple-darwin`, `lipo`, `CMAKE_OSX_ARCHITECTURES`
- [ ] 2.2 DMG mit `hdiutil` (App + Verknüpfung „Programme" + LICENSE)
- [ ] 2.3 Prüfen, dass die App auf einem Intel-Mac startet (oder unter Rosetta: `arch -x86_64`)

## 3. Release-Workflow

- [ ] 3.1 `release.yml` bei Tag `v*`: Version aus Tag gegen `project(VERSION)` prüfen
- [ ] 3.2 macOS- und Windows-Job, Artefakte mit Version im Namen
- [ ] 3.3 Release-Entwurf (Draft) mit Text aus Vorlage (Startanleitung, bekannte Grenzen)
- [ ] 3.4 Probelauf mit Test-Tag `v0.1.1-rc1` auf dem Branch; Entwurf danach löschen

## 4. Dokumentation

- [ ] 4.1 README: Abschnitt „Download", Startanleitung Gatekeeper/SmartScreen, Windows-Hinweis Port-Exklusivität
- [ ] 4.2 Nachtrag ADR-23 (Release-Weg) und ADR-10 (Windows)
- [ ] 4.3 `docs/10-anforderungen.md`: Anforderungen zur Verteilung ergänzen

## 5. Stufe 2: Signieren und Notarisieren (braucht Apple Developer Program)

- [ ] 5.1 André: Apple Developer Program, Zertifikat „Developer ID Application", App-spezifisches Passwort
- [ ] 5.2 André: Secrets im GitHub-Repository hinterlegen (Namen siehe design.md)
- [ ] 5.3 Workflow: Keychain anlegen, `codesign --options runtime`, `notarytool submit --wait`, `stapler staple`
- [ ] 5.4 Hardened Runtime: nötige Entitlements prüfen (keine für CoreMIDI erwartet)
- [ ] 5.5 Ohne Secrets weiter unsigniert bauen (Fork-Test)

## 6. Abnahme vor dem Merge

- [ ] 6.1 Windows 11 (André): Zip entpacken, starten, JD-990 verbinden, Patch lesen und senden, hörbar
- [ ] 6.2 Windows 11: `jd990 hil-sweep` über einen Bereich, 0 Abweichungen
- [ ] 6.3 Windows 11: Editor und DAW gleichzeitig am selben Port (Windows MIDI Services) — Ergebnis dokumentieren
- [ ] 6.4 macOS: DMG auf einem zweiten Mac/Benutzerkonto öffnen und starten
- [ ] 6.5 UI-Referenzbilder auf macOS unverändert (`host/tests/ui_snapshots.sh`)
- [ ] 6.6 `openspec validate add-release-builds`, Merge nach `main`, Change archivieren
