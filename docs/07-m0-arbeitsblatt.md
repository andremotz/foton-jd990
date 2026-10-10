# 07 — M0 Arbeitsblatt

> **Dieses Blatt ist zum Abarbeiten und Ausfüllen da.** Ergebnisse direkt hier
> eintragen und committen — es ist der Übergabepunkt zwischen Sitzungen und
> zwischen Rechnern. Was hier nicht steht, ist nach der nächsten Sitzung weg.

**Stand:** M0 abgeschlossen bis auf C3 und die Hörprobe (Teil A, B, C1, C2, D erledigt — Details im Logbuch (internes Logbuch); Rest läuft nachts nach Runbook (internes Runbook)) · **Rechner:** Mac Studio · **Hardware:** JD-990 am Emagic Unitor8

Ziel von M0 ist **nicht**, Code zu schreiben. Ziel ist, die letzte Unbekannte des
Projekts zu beseitigen: die Adresstabelle und das reale Verhalten des Geräts.
Abnahmekriterium steht in [05-meilensteine.md](05-meilensteine.md).

---

## Teil A · Vorbereitung

Diese Schritte brauchen Administratorrechte oder das Gerät in Reichweite und sind
deshalb Handarbeit.

- [x] **A1 — Unitor Family Driver v3 installieren** — *war bereits installiert (`EmagicUSBMIDIDriver.plugin`), lädt unter macOS 27 ohne Zusatzfreigaben*
      <https://support.apple.com/en-us/106523> · offiziell ab macOS 10.15.7,
      Intel und Apple Silicon. Neustart danach.

      Das `.pkg` fragt nach dem **Administratorpasswort dieses Macs** (das eigene
      Anmeldepasswort, sofern der Benutzer Admin ist), weil es nach
      `/Library/Audio/MIDI Drivers/` schreibt. Handarbeit — ein Passwortdialog ist
      nichts, was automatisch ausgefüllt wird.

      Danach prüfen, ob dort etwas angekommen ist:
      ```bash
      ls -la "/Library/Audio/MIDI Drivers/"
      ```

      → Das ist **Risiko R1**: macOS auf dem Mac Studio ist neuer als die
      Treiberpflege. Mögliche Zusatzhürden, in dieser Reihenfolge wahrscheinlich:
      eine Freigabe unter *Systemeinstellungen → Datenschutz & Sicherheit*; und
      falls es sich doch um eine Kernel-Erweiterung handelt, auf Apple Silicon
      zusätzlich ein Neustart in den Wiederherstellungsmodus, um „Reduzierte
      Sicherheit" zu erlauben. Wenn es an diesem Punkt hakt: **nicht festbeißen**,
      notieren und auf ein klassenkompatibles USB-MIDI-Interface ausweichen. Das
      Projekt darf nicht an einem Treiber von 2005 hängen.
- [x] **A2 — Unitor8 im Audio-MIDI-Setup sichtbar?** Ports korrekt benannt? — *Unitor8 MkII, CoreMIDI zeigt `Unitor8/AMT8 Port 1–8`; JD-990 an Port 2*
- [x] **A3 — Werkzeuge installieren:** [SysEx Librarian](https://www.snoize.com/SysExLibrarian/)
      und [MIDI Monitor](https://www.snoize.com/MIDIMonitor/) (beide kostenlos, Snoize).
- [x] **A4 — Am JD-990 prüfen:** Exclusive auf `ON`, Device-ID notieren.
      Das SysEx-Byte ist **Anzeige minus 1** — Anzeige „17" ergibt `0x10`.
      Alle Nachrichten unten nehmen `0x10` an; bei abweichender ID das dritte
      Byte ersetzen **und die Prüfsumme neu rechnen** (sie deckt nur Adresse und
      Daten ab, nicht die Device-ID — die Nachrichten bleiben also gültig).

Device-ID: `17 → 0x10`  ·  macOS-Version: `27.0 (26A428)`  ·  Treiber läuft: ☑ ja ☐ nein

*Exclusive-Empfang: SYSTEM → Rx MIDI → Rx Exclusive (OFF / ON-1 / ON-2, Handbuch PDF-S. 182) — ist an, sonst hätte B2 nicht geantwortet.*

---

## Teil B · Erste Bytes

### B1 — Antwortet das Gerät überhaupt?

```
F0 7E 10 06 01 F7
```

Universal Identity Request. Antwortet der JD-990, haben wir Geräteerkennung
geschenkt. Antwortet er nicht, ist das kein Fehler — viele Roland-Geräte dieser
Generation können das nicht.

Antwort: ☑ keine ☐ ja → Bytes hier eintragen: `—` *(auch nicht per Broadcast `F0 7E 7F …`; Erkennung muss über RQ1 laufen)*

### B2 — Der Patchname als Probe ⭐

**Der wichtigste Einzelschritt dieses Meilensteins.** Statt gleich einen ganzen
Patch anzufordern, holen wir nur die ersten 16 Bytes des Temporary Patch — den
Namen. Der steht laut Datenstruktur bei Offset 0.

```
F0 41 10 57 11 03 00 00 00 00 00 00 10 6D F7
│  │  │  │  │  └─ Adresse ─┘ └─ Größe ──┘ │
│  │  │  │  └ RQ1                         └ Prüfsumme
│  │  │  └ Model ID JD-990
│  │  └ Device ID
│  └ Roland
└ SysEx
```

Warum diese Probe so viel wert ist: Kommt als Antwort der Patchname in lesbarem
ASCII zurück — derselbe, der auf dem Display steht — dann sind **in einem Schritt
bestätigt**: Model ID, Device ID, Basisadresse des Temporary Patch, das
Prüfsummenverfahren, das Größenfeld und der Namens-Offset. Und das Ergebnis kann
man mit bloßem Auge gegen das Gerätedisplay halten.

Kommt nichts, liegt der Fehler in genau einer dieser sechs Annahmen — und man kann
sie einzeln durchgehen, statt in einem 486-Byte-Dump zu suchen.

Antwort empfangen: ☐ nein ☑ ja · gelesener Name: `Oasis           ` *(Prüfsumme ok, 2026-09-27)*
Stimmt mit dem Display überein: ☐ ja ☐ nein

### B3 — Schreiben in die Gegenrichtung

```
F0 41 10 57 12 03 00 00 00 43 4C 41 55 44 45 20 54 45 53 54 20 20 20 20 20 4F F7
```

DT1 mit 16 ASCII-Bytes: `EDITOR TEST     `. Ändert sich der Name im Display, ist
auch die Schreibrichtung bestätigt.

> Das schreibt **nur in den Temporary-Bereich**. Der interne Speicher bleibt
> unberührt, solange am Gerät nicht „Write" gedrückt wird.

Name ändert sich im Display: *nicht beobachtet (nachts)* — **Rücklesen ergibt `EDITOR TEST     `** ✅ (2026-09-28)

### B4 — Ganzer Temp Patch

```
F0 41 10 57 11 03 00 00 00 00 00 03 66 14 F7
```

486 Bytes, als `00 00 03 66` im 7-Bit-Größenfeld. Falls das Gerät nicht antwortet,
ist die angenommene Patchgröße falsch — dann in Schritten probieren (`00 00 01 00`
= 128 Bytes) und die tatsächliche Größe aus der Antwort ablesen.

**Hier abzulesen und einzutragen:**

| Frage | Antwort |
|---|---|
| Kommt der Dump als eine Nachricht oder mehrere? | mehrere: **3** *(gemessen an Patch Internal `06 nn`, Temp folgt nachts)* |
| Falls mehrere: wie viele Datenbytes je Paket? | **118 + 256 + 112** — max. 256, aber an der Tone-A-Grenze (`0x76`) geschnitten |
| Gesamtlänge der Nutzdaten | **486** ✅ |
| Stimmt Byte 16 (Patch Level) mit dem Gerät überein? | |

---

## Teil C · Verhalten des Geräts

- [x] **C1 — Sendet der JD-990 bei Frontplatten-Edits?** — *nein, 2 Mitschnitte mit 0 Byte (2026-09-28)*
      MIDI Monitor offen lassen, am Gerät einen Parameter verstellen.
      → Entscheidet, ob echte bidirektionale Synchronisation möglich ist oder ob
      es einen „Vom Gerät neu einlesen"-Knopf braucht (R5).
      Sendet: ☐ ja ☑ nein

- [x] **C2 — Ab welcher Senderate verwirft das Gerät Nachrichten?** — *gar nicht, bis Leitungsgeschwindigkeit (2026-09-28)*
      Einzelne DT1 in dichter Folge schicken und zurücklesen. Ergebnis bestimmt
      den Rate Limiter aus ADR-8.
      Grenze: `keine gefunden — 0 ms Abstand (~285 Einzel-DT1/s, MIDI-Limit) ohne Verlust`

- [ ] **C3 — Programmwechsel:** Sendet das Gerät bei Patchwechsel etwas?
      Lädt es den Temp-Bereich neu?

---

## Teil D · Ernten

- [x] **D1 — Alle internen Bänke dumpen** — *2026-09-27: System (38 B) + 64 Patches + 16 Performances (572 B), 81/81 ok → `corpus/2026-09-27-backup/`. **Eigenes Backup-Ziel steht noch aus.** Rhythm descoped.* und als `.syx` sichern.
      Das ist gleichzeitig das **erste Backup des Geräts** (R8: batteriegepuffertes
      RAM, 33 Jahre alt) und das **Testkorpus** für die Round-Trip-Tests aus M1.
      → Ziel: `corpus/` · **Achtung: ist in `.gitignore`.** Braucht ein eigenes
      Backup-Ziel (Time Machine, externes Medium), sonst existiert es nur einmal.

- [x] **D2 — Adresstabelle übertragen.** — *`spec/addresses.toml` (2026-09-28); offene Punkte in 02 beantwortet* Owner's Manual, Anhang „MIDI
      Implementation" → `spec/addresses.toml`. Danach die offenen Punkte aus
      [02-jd990-protokoll.md](02-jd990-protokoll.md) durchgehen.

- [x] **D3 — `verified`-Flags nachziehen.** — *Spec v2: alle 190 Parameter gegen Handbuch geprüft, Rohbereiche ergänzt, 64 Patches im Bereich (`m0/check_spec.py`); viele Korrekturen, siehe Logbuch* Aktuell 7 von 104 Parametern
      verifiziert. Jeder gegen das Handbuch geprüfte Wertebereich wird in
      `spec/params.toml` auf `verified = true` gesetzt.

Prüfbefehl nach Änderungen an der Spec:

```bash
python3 -c "import tomllib; d=tomllib.load(open('spec/params.toml','rb')); a=d['tone']['param']+d['patch']['param']; print(len(a),'Parameter ·',sum(1 for x in a if x['verified']),'verifiziert')"
```

---

## Abnahmekriterium M0

> Ein Skript holt Patch I:11 aus dem Gerät, schreibt ihn in eine `.syx`-Datei,
> sendet genau diese Bytes zurück in den Temp-Bereich — und das Gerät zeigt und
> klingt exakt wie vorher. Die vollständige Adresstabelle liegt als Datei vor.

Erreicht am: `2026-09-28` — Teil 1 (Byte-Round-Trip I:11 und alle 64 Patches) ✅ · Teil 2 (Adresstabelle als Datei) siehe D2

*Hinweis: „klingt exakt wie vorher" wird nachts durch Zurücklesen + byteweisen Vergleich ersetzt (strenger als Hören); das Hören holt André tagsüber nach.*

---

## Was danach kommt

M1: `jd990-core` in Rust, Codec und Modell aus der Spec generiert, Round-Trip-Tests
gegen das Korpus aus D1. Details in [05-meilensteine.md](05-meilensteine.md).

**Erst wenn die Adresstabelle verifiziert ist, darf Code an die Hardware senden.**
Bis dahin gilt für alle Parameter mit `verified = false`: nur anzeigen, nicht senden.
