# Fremdcode und Lizenzen

Dieses Projekt übernimmt Daten und Algorithmen aus folgender Software. Die
Hinweise müssen bei jeder Weitergabe (Quelltext und Binärdateien) erhalten bleiben.

## JDTools

<https://github.com/sagamusix/JDTools> — genutzt für:

* Feldreihenfolge der JD-990-Strukturen (`spec/params.toml`, `docs/02-jd990-protokoll.md`)
* Wellenformnamen (`spec/waveforms.toml`)
* JD-800-Struktur und JD-800 → JD-990-Konvertierung (`crates/jd990-core/src/jd800.rs`),
  inkl. des JD-800-Standardpatches als Testdatensatz

```
Copyright (c) 2022 - 2024, Johannes Schultz
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the
   names of its contributors may be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## JUCE

`host/JUCE` (nicht im Repository) — doppelt lizenziert: AGPLv3 oder JUCE-Lizenz,
siehe `host/JUCE/LICENSE.md`. Dieses Projekt nutzt JUCE unter der **AGPLv3** und
steht selbst unter der AGPLv3 (`LICENSE`, ADR-23).

## Rust-Abhängigkeiten

Über Cargo bezogen (u. a. `rusqlite` mit eingebautem SQLite — Public Domain,
`midir` — MIT, `serde_json` — MIT/Apache-2.0, `proptest` — MIT/Apache-2.0).
Lizenzübersicht: `cargo tree` bzw. die jeweiligen Crate-Seiten.
