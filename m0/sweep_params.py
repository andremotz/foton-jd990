#!/usr/bin/env python3
"""M0-WEGWERFCODE: Parameterliste für `jd sweep` aus der Spec erzeugen (TSV, Offset im Patch)."""
import tomllib, sys
s = tomllib.load(open("spec/params.toml", "rb"))
b = s["meta"]["tone_bases"]
rows = [(p["id"], p["offset"], p.get("bytes", 1), p["raw"]["min"], p["raw"]["max"]) for p in s["patch"]["param"]]
for t in "abcd":
    rows += [(p["id"].replace("tone.", f"tone{t.upper()}."), b[t] + p["offset"], p.get("bytes", 1), p["raw"]["min"], p["raw"]["max"]) for p in s["tone"]["param"]]
print("# id\toffset\tbytes\tmin\tmax")
for r in rows: print("\t".join(map(str, r)))
