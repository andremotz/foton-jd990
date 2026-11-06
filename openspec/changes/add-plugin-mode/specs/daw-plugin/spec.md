# Spec Delta

## Purpose
FotonJD990 als Plugin auf einer DAW-Spur: Jede Instanz bearbeitet genau ein Ziel des
JD-990 (Patch-Modus, einen Part oder die gemeinsame Performance) und speichert es im
DAW-Projekt, während die DAW die Noten selbst zum Gerät schickt.

## ADDED Requirements

### Requirement: Plugin-Formate
FotonJD990 SHALL als VST3 (macOS, Windows) und AU (macOS) ladbar sein und sich als
Instrument mit MIDI-Eingang anmelden. Die Standalone-App MUST weiterhin funktionieren.

#### Scenario: Laden in Bitwig
- **WHEN** FotonJD990 (VST3) auf eine Instrumentspur in Bitwig gelegt wird
- **THEN** öffnet sich der Editor im Plugin-Fenster und verbindet sich mit dem JD-990

### Requirement: Zielbindung
Jede Instanz SHALL an genau ein Ziel gebunden sein: Patch-Modus, Part 1–7 oder
Performance gemeinsam. Ein Ziel MUST zu jeder Zeit höchstens eine bearbeitende Instanz haben.

#### Scenario: Zweite Instanz am selben Part
- **WHEN** Instanz B das Ziel Part 3 wählt, das Instanz A schon bearbeitet
- **THEN** zeigt B einen Hinweis mit „Übernehmen" und sendet nichts
- **AND** nach „Übernehmen" bearbeitet B den Part und A ist schreibgeschützt

### Requirement: Kanal sichtbar und geprüft
Eine Part-Instanz SHALL den Empfangskanal ihres Parts anzeigen und SHALL warnen, wenn auf
ihrem MIDI-Eingang Noten auf einem anderen Kanal ankommen.

#### Scenario: Falscher Spurkanal
- **WHEN** Part 3 auf Kanal 3 empfängt und die Spur Noten auf Kanal 5 liefert
- **THEN** zeigt die Instanz „Spur sendet auf Kanal 5, Part 3 hört auf Kanal 3"

#### Scenario: Kanäle automatisch zuweisen
- **WHEN** der Nutzer „Parts auf Kanal 1–7 legen" wählt
- **THEN** wird der Empfangskanal von Part n in der Temp-Performance auf n gesetzt
- **AND** nichts wird in den internen Speicher geschrieben

### Requirement: Klang im Projekt gespeichert
Eine Instanz SHALL ihren Zielzustand im DAW-Projekt speichern. Beim Öffnen des Projekts
SHALL das Setup aller Instanzen an das Gerät gesendet werden, die Performance vor den
Part-Patches.

#### Scenario: Projekt öffnen
- **WHEN** ein Projekt mit einer Performance-Instanz und drei Part-Instanzen geöffnet wird
- **THEN** schaltet das Gerät bei Bedarf in den Performance-Modus
- **AND** erhält zuerst die Performance, danach die drei Part-Patches
- **AND** klingt jeder Part wie beim Speichern

#### Scenario: Recall abgeschaltet
- **WHEN** die Option „beim Laden nicht senden" aktiv ist
- **THEN** bleibt das Gerät unverändert und die Instanzen zeigen „nicht abgeglichen"

### Requirement: Ein Port-Besitzer
Alle Instanzen auf einem Rechner SHALL über einen einzigen Besitzer mit dem Gerät
sprechen. SysEx-Nachrichten verschiedener Instanzen MUST NOT ineinander geraten, und
Leseanfragen SHALL nacheinander laufen.

#### Scenario: Zwei Instanzen ändern gleichzeitig
- **WHEN** Instanz A den Cutoff von Part 1 und Instanz B den Level von Part 2 zieht
- **THEN** kommen beide Änderungen vollständig und in sich korrekt am Gerät an

### Requirement: Sicherheitsregeln unverändert
Das Plugin MUST dieselbe Schreib-Freigabeliste erzwingen wie die App (ADR-7/22).

#### Scenario: Kein Schreiben in den internen Speicher
- **WHEN** eine Instanz eine Nachricht außerhalb der Freigabeliste erzeugen würde
- **THEN** wird sie vom Port-Besitzer verworfen
