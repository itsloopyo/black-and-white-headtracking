# Locate the leash/collar selection code. Search defined strings for
# leash/lead/collar/teach/discipline keywords and report the functions that
# reference them - the selection/right-click handler should be among them.

import re

st = currentProgram.getSymbolTable()
fmgr = currentProgram.getFunctionManager()
ref = currentProgram.getReferenceManager()
listing = currentProgram.getListing()

KW = re.compile(r"(leash|lead|collar|teach|discipl|compassion|aggress|learn|nexus|rope|tether)", re.I)

di = currentProgram.getListing().getDefinedData(True)
hits = []
for d in di:
    try:
        v = d.getValue()
    except:
        continue
    if v is None:
        continue
    s = str(v)
    if len(s) < 3:
        continue
    if KW.search(s):
        hits.append((d.getAddress(), s))

print("=== string hits ({}) ===".format(len(hits)))
seen_funcs = {}
for a, s in hits[:200]:
    s1 = s.replace("\n", " ")[:60]
    # who references this string?
    callers = []
    for r in ref.getReferencesTo(a):
        fa = r.getFromAddress()
        f = fmgr.getFunctionContaining(fa)
        if f:
            callers.append("{}@0x{:08X}".format(f.getName(), fa.getOffset()))
            seen_funcs.setdefault(f.getName(), f.getEntryPoint().getOffset())
    print("  0x{:08X} {!r:62}  <- {}".format(a.getOffset(), s1, ", ".join(callers[:4])))

print("\n=== distinct referencing functions ===")
for name, entry in sorted(seen_funcs.items(), key=lambda kv: kv[1]):
    print("  0x{:08X} {}".format(entry, name))
