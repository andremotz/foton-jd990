# ADR-10 — Geräteverbindung: fester Port, automatisches Verbinden

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-09-27 (Konfiguration), 2026-09-28 (automatisches Verbinden) |
| **Herkunft** | Studio-Setup André, M0-Arbeitsblatt |

## Kontext

Der JD-990 hängt an einem Unitor8 MkII (USB) am Mac Studio (macOS 27). Er ist
eingestellt auf Rx-Kanal 12, Perform Ctrl 16, Device ID 17 (SysEx `0x10`),
Stack off. SysEx wird über die Device ID adressiert, nicht über den Kanal.

## Entscheidung

* Standard-Port ist **„Unitor8/AMT8 Port 2"** mit Device ID `0x10`. Andere Ports
  werden nicht angesprochen (Test- und Nachtläufe: ausschließlich Port 2).
* Die App **verbindet sich beim Start selbst**, liest den Temp-Patch und
  startet den Abgleich. Fehlt der Port (Interface aus), versucht sie es alle 2 s
  erneut, bis er erscheint.
* Vorhören (▶ Hören) sendet Noten auf dem Empfangskanal, Standard **12**.
* `--offline` öffnet keinen Port (Tests, Arbeit ohne Gerät).

## Konsequenzen

* Kein Setup-Dialog nötig. Ein anderer Port lässt sich in der Liste oben links wählen.
* Weil CoreMIDI mehrere Clients erlaubt, darf nur **eine** App-Instanz laufen:
  Zwei Instanzen würden sich gegenseitig die Antworten auf RQ1 wegnehmen. Die App
  erzwingt das; nur Testläufe (`--snapshot`/`--quit`) dürfen parallel starten.
