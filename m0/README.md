# m0/ — Wegwerf-Skripte aus Meilenstein M0

**Kein Produktivcode — abgelöst.** Zweck war, Fragen an die Hardware zu
beantworten, bevor M1 beginnt. Seit 2026-09-28 übernimmt das Rust-CLI
(`jd990 probe | dump | backup | send-temp | set | hil-sweep`) alle Aufgaben.
Die Skripte bleiben als Beleg für das Logbuch liegen.

```bash
swiftc -O m0/jd.swift -o .build/jd          # einmal bauen (oder: swift m0/jd.swift …)
.build/jd probe                             # Name des Temp-Patches
.build/jd rq 06000000 486                   # RQ1: 4 Adressbytes (hex), Größe (dez)
JD_SYSTEM_SIZE=38 JD_PERF_SIZE=144 .build/jd backup corpus/$(date +%F)-backup
```

Sendet ausschließlich RQ1 (lesend). Port/Device per `JD_PORT` / `JD_DEV`.
Befunde stehen im Logbuch (internes Logbuch).
