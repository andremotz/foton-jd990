# ADR-8 — Synchronisation und Rate Limiting

| | |
|---|---|
| **Status** | angenommen |
| **Datum** | 2026-09-20 |
| **Herkunft** | Planungsphase, ursprünglich in `docs/03-architektur.md` |

**Entscheidung:** Zwischen UI und Transport liegt eine Sende-Warteschlange mit:
* **Coalescing** — pro Parameter gilt nur der zuletzt gesetzte Wert;
  ein Fader-Schwenk erzeugt keinen Rückstau
* **Globaler Ratenbegrenzung** — Byte-Budget, konfigurierbar,
  Default ~60 % von 3.125 B/s
* **Prioritäten** — einzelne Parameteränderungen vor Bulk-Dumps

**Sync-Modell:**
* Der Editor ist **Owner** des Temp-Bereichs. Seine Sicht gilt.
* Auf Programmwechsel (gesendet oder empfangen) → Temp-Patch neu anfordern.
* Frontplatten-Edits am Gerät: abhängig von M0-Frage 3. Falls das Gerät nicht
  sendet → sichtbarer „Vom Gerät neu einlesen"-Button statt stiller
  Divergenz. **Lieber ehrlich unsynchron als falsch synchron.**

---

## Nachtrag 2026-09-28 — Umsetzung und Messwerte

* **Warteschlange** (`jd990-midi/queue.rs`, im Host `DeviceLink`): Coalescing je
  Adresse, Token-Bucket 60 % von 3125 B/s, Prioritäten.
* **Gerätetiming (gemessen):** Nach einem DT1 in den Patch-Common-Block braucht
  der JD-990 ≥ 80 ms, bevor er die nächste Nachricht annimmt (70 ms: 0/5, 80 ms:
  5/5). Deshalb `common_settle` = 150 ms, sonst 20 ms Abstand.
* **Frontplatten-Edits sendet das Gerät nicht** (M0-Frage 3 geklärt). Umgesetzt
  ist deshalb der ehrliche Sync-Status in der Statusleiste plus „Vom Gerät lesen"
  und ein optionaler **Abgleich**: Alle 5 s wird der Temp-Patch leise per RQ1
  gelesen und verglichen, nie während einer laufenden Übertragung.
* ⏳ **Offen:** André hat das 5-s-Senden bemerkt. Zur Wahl stehen: Intervall
  einstellbar, standardmäßig aus, oder nur abgleichen, während die App im
  Hintergrund ist (Empfehlung). Noch nicht entschieden.
