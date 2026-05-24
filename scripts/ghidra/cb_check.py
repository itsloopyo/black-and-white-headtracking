fact = currentProgram.getAddressFactory()
af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
RENDER = 0x0054DA80
def callers_of(entry):
    f = af.getFunctionAt(addr(entry)); out=set()
    if not f: return out
    for r in ref.getReferencesTo(f.getEntryPoint()):
        cf = af.getFunctionContaining(r.getFromAddress())
        if cf: out.add(cf.getEntryPoint().getOffset())
    return out
def reaches(start, target, max_depth=14):
    seen={start}; fr=[(start,[start])]
    while fr:
        n,p=fr.pop()
        if len(p)>max_depth: continue
        for c in callers_of(n):
            if c==target: return p+[target]
            if c not in seen: seen.add(c); fr.append((c,p+[c]))
    return None
for t in (0x00819920, 0x00819F50):
    p = reaches(t, RENDER)
    print("0x{:08X} reachable from render: {}".format(t, " <- ".join("0x%08X"%x for x in p) if p else "NO"))
