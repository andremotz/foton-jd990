# Proposal

## Why

FotonJD990 lässt sich bisher nur aus dem Quellcode bauen (Xcode-Tools, CMake, Rust,
JUCE-Klon). Für das Video und für Nutzer ohne Entwicklerwerkzeuge braucht es
fertige Downloads: eine macOS-App und eine Windows-`.exe`, automatisch gebaut und
an ein GitHub-Release gehängt.

## What Changes

- GitHub-Actions-Workflow `release.yml`: baut bei einem Versions-Tag (`v*`) die
  macOS-App (Universal: arm64 + x86_64) und den Windows-Build (x64) und hängt sie
  an ein **Entwurfs-Release** (Draft), das André von Hand freigibt.
- Der bestehende CI-Workflow baut zusätzlich auf `windows-latest`, damit ein
  kaputter Windows-Build sofort auffällt.
- Windows-Port: CMake/Rust-Kern für MSVC, MIDI-Portnamen und automatisches
  Verbinden (ADR-10) unter Windows, Einstellungsordner unter `%APPDATA%`.
- Erste Ausgaben **unsigniert**, mit Startanleitung (Gatekeeper/SmartScreen) in
  README und Release-Text.
- Stufe 2 (eigene Aufgabengruppe, braucht das Apple Developer Program): macOS mit
  Developer ID signieren und notarisieren, Secrets im Repository.
- Versionsnummer kommt aus einer Quelle (CMake `project(VERSION)` = Tag).

**Nicht dabei:** Mac App Store, Windows-Code-Signing-Zertifikat (später, eigener
Change), Linux-Pakete, Auto-Update, Plugin-Formate (Change `add-plugin-mode`).

**Branch:** `ci/release-builds`. Merge nach `main` erst nach der Abnahme
(Aufgabengruppe 6), inklusive Hardwaretest unter Windows 11.

## Capabilities

### New Capabilities
- `release-distribution`: Wie fertige, installierbare Builds entstehen, was sie
  enthalten und wie sie veröffentlicht werden.

### Modified Capabilities
- (keine)

## Impact

- `.github/workflows/ci.yml`, neu `.github/workflows/release.yml`
- `host/CMakeLists.txt` (Universal-Build, MSVC, Version), `crates/jd990-ffi` (Windows-Staticlib)
- `host/Source/DeviceLink.cpp`, `Model.cpp` (Portnamen, Einstellungsordner)
- README (Download- und Startanleitung), ADR-23 (Nachtrag), ADR-10 (Nachtrag Windows)
- Kosten Stufe 2: Apple Developer Program 99 $/Jahr
