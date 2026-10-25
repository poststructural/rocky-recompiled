"""Find function-pointer tables the function detector missed.

Lists every dword in the data sections that points into one of the given code
sections but not at a detected function start. Bink keeps its pixel and plane
routines in tables like this (in BINKDATA), and calls them indirectly; each
one missing is a skipped call (a black movie, in Bink's case).

    python scripts/find_table_pointers.py [code-section-prefix ...]   (default BINK XPP)

Targets on a 16-byte boundary in a table are almost always real functions;
odd targets in .rdata are usually data that happens to look like an address.
"""
import json
import os
import struct
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")
d = open(os.path.join(ROOT, "game", "default.xbe"), "rb").read()
base = 0x10000
nsec = struct.unpack_from("<I", d, 0x11C)[0]
sh = struct.unpack_from("<I", d, 0x120)[0] - base
secs = []
for i in range(nsec):
    fl, va, vs, ra, rs, nameaddr = struct.unpack_from("<6I", d, sh + i * 56)
    no = nameaddr - base
    secs.append((d[no:d.index(b"\0", no)].decode(), va, vs, ra, rs))

funcs = json.load(open(os.path.join(ROOT, "tools", "ngbrecomp", "xboxrecomp", "tools",
                                    "disasm", "output", "functions.json")))
starts = {int(f["start"], 16) for f in funcs}
prefixes = sys.argv[1:] or ["BINK", "XPP"]
code = [s for s in secs if s[0] != "BINKDATA" and any(s[0].startswith(p) for p in prefixes)]

found = {}
for name, va, vs, ra, rs in secs:
    if name not in (".data", ".rdata", "BINKDATA", "DOLBY"):
        continue
    for off in range(0, rs - 3, 4):
        v = struct.unpack_from("<I", d, ra + off)[0]
        for cn, cva, cvs, _, _ in code:
            if cva <= v < cva + cvs and v not in starts:
                found.setdefault(v, []).append((name, va + off, cn))
for v in sorted(found):
    refs = ", ".join(f"{n}@0x{a:08X}" for n, a, _ in found[v][:3])
    print(f"0x{v:08X}  {found[v][0][2]:<9} {'aligned' if v % 16 == 0 else '       '}  {refs}")
