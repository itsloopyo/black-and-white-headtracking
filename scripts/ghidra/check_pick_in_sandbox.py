# Is the citadel collar pick reachable from the render dispatcher FUN_0054DA80
# (i.e. runs inside the sandbox where g_cameraStruct = rotated)? Do a bounded
# upward caller walk from the pick functions and report if FUN_0054DA80 is an
# ancestor. Also print the immediate caller chain.

fact = currentProgram.getAddressFactory()
af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

RENDER = 0x0054DA80
TARGETS = [0x00519960, 0x00466730, 0x00819390]

def callers_of(entry):
    f = af.getFunctionAt(addr(entry))
    out = set()
    if not f: return out
    for r in ref.getReferencesTo(f.getEntryPoint()):
        cf = af.getFunctionContaining(r.getFromAddress())
        if cf:
            out.add(cf.getEntryPoint().getOffset())
    return out

def reaches_render(start, max_depth=12):
    # BFS upward to see if RENDER is an ancestor.
    seen = set([start])
    frontier = [(start, [start])]
    while frontier:
        node, path = frontier.pop()
        if len(path) > max_depth:
            continue
        for c in callers_of(node):
            if c == RENDER:
                return path + [RENDER]
            if c not in seen:
                seen.add(c)
                frontier.append((c, path + [c]))
    return None

for t in TARGETS:
    f = af.getFunctionContaining(addr(t))
    nm = f.getName() if f else "?"
    print("=== {} 0x{:08X} ===".format(nm, t))
    cs = callers_of(t)
    for c in sorted(cs):
        cf = af.getFunctionAt(addr(c))
        print("   caller 0x{:08X} {}".format(c, cf.getName() if cf else "?"))
    path = reaches_render(t)
    if path:
        print("   *** REACHABLE FROM FUN_0054DA80 (render sandbox) via:")
        print("       " + " <- ".join("0x{:08X}".format(p) for p in path))
    else:
        print("   not reachable from FUN_0054DA80 within depth limit")
