# 03 — Architektur

## Leitgedanke

> Die Technologie-Entscheidung, bei der man sich am ehesten vergreift
> (SwiftUI vs. Plugin vs. Web), ist die, die man am längsten aufschieben kann.

Deshalb: **Erst den Kern bauen, der in jeder Variante identisch ist.** Die
Host-/UI-Entscheidung fällt erst zu Meilenstein M3 — dann mit einem laufenden,
getesteten Fundament in der Hand statt auf Basis einer Vermutung.

## Schichtenmodell

```
┌──────────────────────────────────────────────────────────────┐
│  Host-Layer          Standalone · VST3 · AU · CLAP           │  ← M6
│                      (oder: SwiftUI-App)                     │
├──────────────────────────────────────────────────────────────┤
│  UI-Layer            JD-800-Panel · Tone-Editor · Librarian  │  ← M4/M5
│                      Generisches Spec-getriebenes UI         │  ← M3
├──────────────────────────────────────────────────────────────┤
│  Session-Layer       Edit-Buffer · Undo/Redo · Sync-State    │  ← M2
│                      Rate Limiter · Coalescing               │
├──────────────────────────────────────────────────────────────┤
│  Transport-Layer     MIDI In/Out · Device Discovery          │  ← M2
├──────────────────────────────────────────────────────────────┤
│  jd990-core          Parameter-Spec · SysEx-Codec            │  ← M1
│  (portabel, UI-frei) Patch-Modell · Library · Diff/Merge     │
├──────────────────────────────────────────────────────────────┤
│  params.toml         DIE Single Source of Truth              │  ← M0/M1
└──────────────────────────────────────────────────────────────┘
```

Regel: **Jede Schicht kennt nur die darunter.** `jd990-core` kennt weder MIDI
noch UI — das macht ihn vollständig ohne Hardware testbar.

---

## Architekturentscheidungen

Die ADRs stehen seit 2026-09-29 einzeln unter [`docs/adr/`](adr/README.md)
(je Datei Status, Datum, Herkunft, Nachträge). Die Nummern sind unverändert,
Verweise wie „ADR-6" gelten weiter.

| ADR | Kurzfassung |
|---|---|
| [1](adr/0001-headless-core-zuerst.md) | Headless Core zuerst, UI später |
| [2](adr/0002-rust-mit-c-abi.md) | Core in Rust mit C-ABI |
| [3](adr/0003-spec-als-datei.md) ⭐ | Parameter-Spec als Datei, Codegen |
| [4](adr/0004-host-juce.md) | Host: JUCE (immer aktuell) |
| [5](adr/0005-syx-und-sqlite.md) | `.syx` kanonisch, SQLite als Bibliothek |
| [6](adr/0006-roundtrip-und-hil.md) ⭐ | Round-Trip + Hardware-in-the-Loop |
| [7](adr/0007-temp-bereich-heilig.md) | Nur Temp-Bereich beschreiben |
| [8](adr/0008-sync-und-rate-limit.md) | Warteschlange, Rate Limit, ehrlicher Sync |
| [9–20](adr/README.md) | Scope, Verbindung, Hardware-Tests, Host-Model, Temp-Besitz, Undo, UI-Tests, Zoom, grafisches Bearbeiten, Annahmen, Bibliothek, CI |

Die Anforderungen stehen in [10-anforderungen.md](10-anforderungen.md).

---

## Modulschnitt (Stand 2026-09-29)

```
jd990-editor/
├── spec/
│   ├── params.toml           # Single Source of Truth (ADR-3): 190 Parameter, Tabellen, [init]
│   ├── addresses.toml        # Adressbereiche und Blockgrößen, in M0 verifiziert
│   └── waveforms.toml        # „None" + 195 Wellenformen
├── crates/
│   ├── jd990-spec/           # Spec-Parser + Codegen (build.rs)
│   ├── jd990-core/           # Patch-Modell, SysEx, Anzeige, Geräteregeln, Variation/Morph,
│   │                         #   Ähnlichkeit, JD-800-Konvertierung, SMF  ← keine I/O
│   ├── jd990-midi/           # Transport (midir), Sende-Warteschlange (ADR-8)
│   ├── jd990-library/        # SQLite-Bibliothek (ADR-5)
│   ├── jd990-cli/            # `jd990` — Werkzeug, Backup, HIL-Sweep (ADR-6)
│   └── jd990-ffi/            # C-ABI für den Host (include/jd990.h)
├── host/                     # JUCE-App (ADR-4, ADR-12)
│   ├── Source/               # Model, DeviceLink, Panels, Controls, Envelope, LibraryView …
│   └── tests/                # UI-Referenzbilder (ADR-15)
├── design/                   # Panel-Prototyp (HTML), Icon
├── m0/                       # Wegwerf-Werkzeuge aus M0 (Swift/Python)
├── corpus/                   # echte Dumps — gitignored (ADR-11)
├── reference/                # Handbuch — gitignored
└── docs/                     # 01–10, adr/
```
