# ADR-6 — Verifikation: Round-Trip + Hardware-in-the-Loop

| | |
|---|---|
| **Status** | angenommen, umgesetzt (HIL 463/463) |
| **Datum** | 2026-09-20 |
| **Herkunft** | Planungsphase, ursprünglich in `docs/03-architektur.md` |

**Entscheidung:** Zwei automatisierte Testebenen, beide ab M1/M2 verpflichtend.

**1. Round-Trip-Tests (ohne Hardware)**
```
parse(bytes) → serialize → bytes'      muss gelten:  bytes == bytes'
```
gegen ein Korpus echter Dumps: die 192 Werkspatches plus frei verfügbare
Bank-Sammlungen. Findet jeden Feldbreiten- und Reihenfolgefehler.
Ergänzt um Property-Tests: zufällige gültige Patches → encode → decode → gleich.

**2. Hardware-in-the-Loop-Sweep (mit JD-990)**

Ein CLI-Kommando iteriert über die **komplette Parameter-Spec** und prüft je
Parameter:
```
für jeden Parameter p:
    für v in [min, max, mid, 3× zufällig]:
        DT1  p = v       an das Gerät
        RQ1  Temp Patch  vom Gerät
        assert gelesener Wert == v
        assert kein anderer Parameter hat sich verändert   ← findet Adressfehler!
```

**Begründung:** Die letzte Zeile ist der eigentliche Wert. Ein falscher
Adress-Offset schreibt in den Nachbarparameter — ein Fehler, den weder
Handbuchlesen noch Hören zuverlässig findet, der aber hier in Sekunden
auffällt. Der JD-990 steht ohnehin im Studio; dieser Vorteil ist geschenkt.

**Konsequenz:** Ein Durchlauf dauert (bei ~486 Parametern × 6 Werten ×
~165 ms Dump) mehrere Minuten. Optimierung: statt Volldump nur den
betroffenen Block anfordern. Läuft als Nightly, nicht bei jedem Commit.
