# Design

## Context

- CI heute (`ci.yml`): Rust auf Ubuntu, macOS-App-Build ohne Artefakt. Hardwaretests
  laufen nie in der CI (ADR-6, ADR-11).
- Der Rust-Kern wird von CMake als Static Library gebaut (`cargo build -p jd990-ffi`).
- Windows wurde nie gebaut. JUCE und Rust sind plattformübergreifend; offen sind
  MSVC-Linkerflags der Rust-Staticlib, Portnamen und das Verhalten des Unitor8 unter
  Windows.
- **Windows MIDI Services** (seit Februar 2026 in Windows 11 24H2/25H2): Jeder
  MIDI-1.0-Port ist mehrfach nutzbar. Ältere Windows-Versionen erlauben pro Port nur
  ein Programm — dann können DAW und Editor nicht gleichzeitig am JD-990-Port hängen.
- André hat einen Windows-11-Rechner für den Hardwaretest.

## Goals / Non-Goals

**Goals:**
- Ein Tag `vX.Y.Z` erzeugt ohne Handarbeit einen Release-Entwurf mit
  `FotonJD990-X.Y.Z-macOS.dmg` und `FotonJD990-X.Y.Z-Windows-x64.zip`.
- Jeder Push prüft, dass Windows weiterhin baut.
- Stufe 2: Die macOS-App startet auf einem fremden Mac ohne Gatekeeper-Warnung.

**Non-Goals:**
- Installer mit Deinstallationsroutine (Windows: Zip mit `.exe` reicht zunächst).
- Signierte Windows-Builds, ARM64-Windows, Linux-Artefakte.

## Decisions

1. **Release als Draft.** Der Workflow veröffentlicht nie selbst; André prüft und
   gibt frei. Ein veröffentlichtes Release ist öffentlich und nicht rückholbar
   gecacht.
2. **macOS Universal Binary** (`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`); der
   Rust-Kern wird für beide Targets gebaut und mit `lipo` vereint. Alternative
   „nur arm64" verworfen: Intel-Macs sind unter JD-990-Besitzern verbreitet.
3. **DMG mit `hdiutil`** (Bordmittel), kein zusätzliches Werkzeug.
4. **Windows als Zip** mit `FotonJD990.exe` und `LICENSE`/`THIRD-PARTY-NOTICES.md`.
   Die MSVC-Runtime wird statisch gelinkt (`/MT`), damit keine VC-Redist nötig ist.
5. **JUCE-Version an einer Stelle:** 9.0.2 als Variable im Workflow, wie im README.
6. **Signieren erst in Stufe 2** über Secrets (`MACOS_CERT_P12`, `MACOS_CERT_PASSWORD`,
   `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`). Ohne Secrets läuft der
   Workflow weiter und baut unsigniert — Forks bleiben baubar.
7. **Einstellungen unter Windows** in `%APPDATA%\JD-990 Editor\` (JUCE
   `userApplicationDataDirectory`), gleicher Ordnername wie unter macOS.

## Risks / Trade-offs

- **Unsignierte Builds** wirken abschreckend (Gatekeeper „Trotzdem öffnen",
  SmartScreen „Weitere Informationen"). Abgefedert durch Anleitung; gelöst durch Stufe 2.
- **MIDI-Port-Exklusivität** auf Windows ohne Windows MIDI Services: Editor und DAW
  am selben Port gehen nicht. Wird im README dokumentiert; Workaround: zweites Interface
  oder Windows-Update.
- **Unitor8 unter Windows:** Treiberlage ungeprüft. Fallback (M0): klassenkompatibles
  USB-MIDI-Interface.
- **Timing unter Windows** (Rate Limit ADR-8, Wartezeiten aus P0) kann abweichen →
  HIL-Sweep unter Windows ist Teil der Abnahme.
