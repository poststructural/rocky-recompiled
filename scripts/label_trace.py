"""Label-by-label trace of one lifted function, to compare with the original.

When tools/recomp/difftest.py reports a mismatch, this finds the first place
where the lifted code and the original part ways:

    python scripts/label_trace.py instrument 0x000E3270
        adds "rky_trace(VA, ebp);" after every label of the
        function in src/recomp/gen (rebuild, no regen; regen removes it).
        Run with RKY_TRACE_VA=0x000E3270 RKY_TRACE_CALL=<n> RKY_TRACE_FILE=...
        to write "va eax ecx edx ebx esp ebp esi edi" per label for call n.
    python scripts/label_trace.py compare <lifted.txt> <original.txt>
        first differing line, with context.

The original side comes from work/difftest/difftest.py with LABEL_TRACE,
LABEL_SRC (the instrumented chunk), LABEL_LO and LABEL_HI set (same format).
"""
import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")


def instrument(va):
    name = f"sub_{va:08X}"
    for path in glob.glob(os.path.join(ROOT, "src", "recomp", "gen", "recomp_*.c")):
        lines = open(path).read().split("\n")
        for i, l in enumerate(lines):
            if re.match(rf"^void {name}(_gen)?\(void\)$", l):
                break
        else:
            continue
        end = next(j for j in range(i + 1, len(lines)) if lines[j].startswith("}"))
        out = lines[:i] + ["extern void rky_trace(uint32_t va, uint32_t ebp);"]
        n = 0
        for j in range(i, end + 1):
            out.append(lines[j])
            m = re.match(r"^loc_([0-9A-F]{8}): ;$", lines[j])
            if m:
                out.append(f"    rky_trace(0x{m.group(1)}u, ebp);")
                n += 1
        out += lines[end + 1:]
        open(path, "w").write("\n".join(out))
        print(f"{n} labels instrumented in {os.path.basename(path)}")
        return
    sys.exit(f"{name} not found")


def compare(a, b):
    la = [l.split() for l in open(a)]
    lb = [l.split() for l in open(b)]
    for i, (x, y) in enumerate(zip(la, lb)):
        if x != y:
            print(f"first difference at line {i}")
            for j in range(max(0, i - 4), min(i + 4, len(la), len(lb))):
                print(f"{j:6} lifted   {' '.join(la[j])}")
                print(f"{'':6} original {' '.join(lb[j])}")
            return
    print(f"no difference in {min(len(la), len(lb))} lines ({len(la)} vs {len(lb)})")


if __name__ == "__main__":
    if sys.argv[1] == "instrument":
        instrument(int(sys.argv[2], 16))
    else:
        compare(sys.argv[2], sys.argv[3])
