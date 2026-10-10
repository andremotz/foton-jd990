#!/usr/bin/env python3
"""M0-WEGWERFCODE: Spec-Invarianten prüfen und das Backup gegen die Spec dekodieren.

    python3 m0/check_spec.py [corpus/<backup-verzeichnis>]

1. Abdeckung: jedes Byte des Tone-Blocks (0–91) und des Patch Common (0–117)
   genau einmal belegt (Name = 0–15).
2. Tabellen: Enum-/Tabellenlänge == raw.max + 1 bzw. passend zum Bereich.
3. Daten: alle Parameter aller Patches im Backup liegen in raw.min…raw.max.
Wird in M1 durch den Codegen + Rust-Tests ersetzt.
"""
import sys, tomllib, glob, collections

spec = tomllib.load(open("spec/params.toml", "rb"))
enums, tables = spec["enums"], spec.get("tables", {})
errors = []

def cover(params, size, reserved=()):
    seen = collections.Counter(reserved)
    for p in params:
        for i in range(p.get("bytes", 1)):
            seen[p["offset"] + i] += 1
    missing = [o for o in range(size) if seen[o] == 0]
    double = [o for o, n in seen.items() if n > 1]
    return missing, double

m, d = cover(spec["tone"]["param"], 92)
if m or d: errors.append(f"Tone-Abdeckung: fehlt {m}, doppelt {d}")
m, d = cover(spec["patch"]["param"], 118, reserved=range(16))
if m or d: errors.append(f"Patch-Common-Abdeckung: fehlt {m}, doppelt {d}")

def table_len(t):
    if "values" in t: return len(t["values"])
    return None  # segmentiert: Länge ergibt sich aus raw.max, nur Startpunkte prüfen

for p in spec["tone"]["param"] + spec["patch"]["param"]:
    disp, raw = p["display"], p["raw"]
    n = raw["max"] + 1
    if disp["kind"] == "enum":
        k = len(enums[disp["table"]]["values"])
        if k != n: errors.append(f"{p['id']}: Enum {disp['table']} hat {k} Werte, raw erlaubt {n}")
    elif disp["kind"] == "table":
        t = tables[disp["table"]]
        k = table_len(t)
        if k is not None and k != n: errors.append(f"{p['id']}: Tabelle {disp['table']} hat {k} Werte, raw erlaubt {n}")
        if "segments" in t:
            last = t["segments"][-1]["from"]
            end = t["notes"]["from"] + len(t["notes"]["values"]) if "notes" in t else n
            if last >= n or end != n: errors.append(f"{p['id']}: Segmente von {disp['table']} passen nicht zu raw.max {raw['max']}")
    elif disp["kind"] == "linear" and "extra" in disp:
        span = raw["max"] - raw["min"] + 1 - len(disp["extra"])
        if span <= 0: errors.append(f"{p['id']}: extra passt nicht")

print(f"Spec: {len(spec['tone']['param'])} Tone- + {len(spec['patch']['param'])} Patch-Parameter · "
      f"{sum(p['verified'] for p in spec['tone']['param'] + spec['patch']['param'])} verified")
for e in errors: print("  SPEC-FEHLER:", e)

# ── Backup dekodieren ───────────────────────────────────────────────────────
backup = sys.argv[1] if len(sys.argv) > 1 else "corpus/2026-09-27-backup"
def patch_data(path):
    b = open(path, "rb").read()
    mem = {}
    for part in b.split(b"\xf7"):
        j = part.find(b"\xf0")
        if j < 0: continue
        msg = part[j:]
        a = (msg[5] << 21 | msg[6] << 14 | msg[7] << 7 | msg[8]) & 0x3FFF
        for k, x in enumerate(msg[9:-1]): mem[a + k] = x
    return bytes(mem[i] for i in range(486))

bases = spec["meta"]["tone_bases"]
files = sorted(glob.glob(f"{backup}/patch-I??.syx"))
viol = collections.defaultdict(list)
stats = collections.defaultdict(lambda: [999, -1])
def val(data, off, nbytes): return data[off] * 128 + data[off + 1] if nbytes == 2 else data[off]
for f in files:
    data = patch_data(f)
    name = data[:16].decode("ascii", "replace")
    if any(c < 0x20 or c > 0x7F for c in data[:16]): viol["patch.name"].append((f, data[:16]))
    checks = [(p, p["offset"]) for p in spec["patch"]["param"]]
    for t in "abcd":
        checks += [(p, bases[t] + p["offset"]) for p in spec["tone"]["param"]]
    for p, off in checks:
        v = val(data, off, p.get("bytes", 1))
        s = stats[p["id"]]; s[0] = min(s[0], v); s[1] = max(s[1], v)
        if not (p["raw"]["min"] <= v <= p["raw"]["max"]):
            viol[p["id"]].append((name.strip(), v))

print(f"Backup: {len(files)} Patches aus {backup}")
if not viol: print("  Alle Werte im Bereich ✅")
for pid, lst in viol.items():
    print(f"  AUSSERHALB: {pid}: {len(lst)}× z. B. {lst[:4]}")
if "-v" in sys.argv:
    for pid, (lo, hi) in stats.items(): print(f"    {pid:28s} beobachtet {lo}…{hi}")
sys.exit(1 if errors or viol else 0)
