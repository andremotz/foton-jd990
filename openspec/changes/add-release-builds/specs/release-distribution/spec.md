# Spec Delta

## Purpose
Fertige, installierbare Builds von FotonJD990 für macOS und Windows, automatisch aus
einem Versions-Tag erzeugt und über GitHub Releases verteilt.

## ADDED Requirements

### Requirement: Release aus Versions-Tag
Ein Push eines Tags der Form `vX.Y.Z` SHALL einen GitHub-Release-Entwurf erzeugen, der
eine macOS-Datei und eine Windows-Datei mit der Version im Namen enthält. Der Workflow
MUST das Release nicht selbst veröffentlichen.

#### Scenario: Tag erzeugt Entwurf
- **WHEN** André den Tag `v0.2.0` pusht
- **THEN** entsteht ein Release-Entwurf `v0.2.0` mit `FotonJD990-0.2.0-macOS.dmg` und `FotonJD990-0.2.0-Windows-x64.zip`
- **AND** das Release ist nicht öffentlich, bis André es freigibt

#### Scenario: Version stimmt nicht
- **WHEN** der Tag nicht zur Version in `host/CMakeLists.txt` passt
- **THEN** bricht der Workflow mit einer verständlichen Meldung ab und erzeugt kein Release

### Requirement: Lauffähig ohne Entwicklerwerkzeuge
Die macOS-App SHALL auf Apple Silicon und Intel ab macOS 13 starten, die Windows-`.exe`
auf Windows 11 x64 ohne zusätzlich installierte Laufzeitbibliotheken.

#### Scenario: Frischer Windows-Rechner
- **WHEN** die Zip-Datei auf einem Windows 11 ohne Visual Studio entpackt und `FotonJD990.exe` gestartet wird
- **THEN** startet der Editor und listet die MIDI-Ports

### Requirement: Gleiche Sicherheitsregeln auf allen Plattformen
Jeder ausgelieferte Build MUST dieselbe Schreib-Freigabeliste erzwingen wie der macOS-Build
(nur Temp-Bereiche und System mode, ADR-7/22).

#### Scenario: Windows-Build schreibt nie in den internen Speicher
- **WHEN** der Windows-Build eine DT1-Nachricht an eine Adresse außerhalb der Freigabeliste senden würde
- **THEN** wird sie verworfen, wie unter macOS

### Requirement: Ehrlicher Hinweis auf Signatur-Status
Solange ein Build nicht signiert ist, SHALL der Release-Text und das README erklären, wie man
ihn trotz Gatekeeper- bzw. SmartScreen-Warnung startet.

#### Scenario: Unsignierte macOS-App
- **WHEN** ein Release ohne Apple-Signatur erstellt wird
- **THEN** enthält der Release-Text die Anleitung „Systemeinstellungen › Datenschutz & Sicherheit › Trotzdem öffnen"

### Requirement: Signierte und notarisierte macOS-App (Stufe 2)
Sind die Signatur-Secrets im Repository hinterlegt, SHALL die macOS-App mit Developer ID
signiert, notarisiert und gestapelt (stapled) werden. Fehlen sie, MUST der Workflow
unsigniert weiterbauen.

#### Scenario: Fremder Mac
- **WHEN** ein signiertes Release auf einem Mac ohne Entwicklerwerkzeuge geöffnet wird
- **THEN** startet die App ohne Gatekeeper-Warnung

### Requirement: Windows-Build in der laufenden CI
Jeder Push und jeder Pull Request SHALL den Windows-Build ausführen, damit Windows nicht
unbemerkt bricht.

#### Scenario: Windows bricht
- **WHEN** eine Änderung unter MSVC nicht kompiliert
- **THEN** schlägt der CI-Lauf fehl
