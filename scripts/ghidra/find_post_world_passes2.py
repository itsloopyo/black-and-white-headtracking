# Walk UP from the world-render wrapper FUN_0054D850 to the real frame loop,
# then dump the ordered call sequence of that frame loop, flagging calls that
# happen AFTER the world-render call (post-world passes: HUD, name labels,
# markers) which read a camera matrix and/or draw.

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

WORLD_WRAPPER = 0x0054D850
CAMERA_STATE = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
}
DEVICE = 0x00ECA638

def analyze(ep):
    f = fm.getFunctionAt(addr(ep))
    if not f: return (set(), False)
    reads = set(); dev = False
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t = r.getToAddress().getOffset()
                if t in CAMERA_STATE: reads.add(CAMERA_STATE[t])
                if t == DEVICE: dev = True
    return (reads, dev)

def callers_of(ep):
    out = set()
    for r in ref.getReferencesTo(addr(ep)):
        if r.getReferenceType().isCall():
            f = fm.getFunctionContaining(r.getFromAddress())
            if f: out.add(f.getEntryPoint().getOffset())
    return out

# Build the chain upward until we hit a function that makes other draw/render
# calls besides the world wrapper (the real frame loop).
chain = [WORLD_WRAPPER]
cur = WORLD_WRAPPER
for _ in range(6):
    cs = callers_of(cur)
    print("callers of FUN_{:08X}: {}".format(cur, [hex(c) for c in cs]))
    if not cs: break
    cur = sorted(cs)[0]
    chain.append(cur)

def dump_frame(caller_ep, marker_ep):
    f = fm.getFunctionAt(addr(caller_ep))
    print("\n" + "=" * 78)
    print("Frame loop FUN_{:08X} (size={}) - calls after FUN_{:08X}:".format(
        caller_ep, int(f.getBody().getNumAddresses()), marker_ep))
    print("=" * 78)
    seen_marker = False
    n = 0
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            if not r.getReferenceType().isCall(): continue
            tgt = fm.getFunctionContaining(r.getToAddress())
            tep = tgt.getEntryPoint().getOffset() if tgt else r.getToAddress().getOffset()
            if tep == marker_ep:
                seen_marker = True
                print("  @{}  -> FUN_{:08X}  <== WORLD RENDER".format(ins.getAddress(), tep))
                continue
            reads, dev = analyze(tep) if tgt else (set(), False)
            tag = "AFTER" if seen_marker else "before"
            extra = (" [DRAWS]" if dev else "") + (" reads={}".format(sorted(reads)) if reads else "")
            print("  {} @{} -> FUN_{:08X}{}".format(tag, ins.getAddress(), tep, extra))
            n += 1

# Dump every function in the chain so we can see where post-world passes live.
for i in range(1, len(chain)):
    dump_frame(chain[i], chain[i-1])

print("\nDone.")
