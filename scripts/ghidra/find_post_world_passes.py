# Find the frame loop(s) that call the world-render dispatcher FUN_0054DA80,
# then dump the ORDERED sequence of calls each such caller makes. Calls that
# occur AFTER the FUN_0054DA80 call site are post-world passes (HUD, name
# labels, markers) that run outside our render sandwich and see the clean
# g_cameraStruct -> drift suspects.

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

WORLD = 0x0054DA80
CAMERA_STATE = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
}
DEVICE = 0x00ECA638

def cam_reads_and_device(ep):
    f = fm.getFunctionAt(addr(ep))
    if not f: return (set(), False)
    reads = set()
    dev = False
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t = r.getToAddress().getOffset()
                if t in CAMERA_STATE: reads.add(CAMERA_STATE[t])
                if t == DEVICE: dev = True
    return (reads, dev)

# Callers of WORLD
callers = set()
for r in ref.getReferencesTo(addr(WORLD)):
    if r.getReferenceType().isCall():
        f = fm.getFunctionContaining(r.getFromAddress())
        if f: callers.add(f.getEntryPoint().getOffset())

print("Callers of FUN_{:08X}: {}".format(WORLD, [hex(c) for c in callers]))

for caller_ep in sorted(callers):
    f = fm.getFunctionAt(addr(caller_ep))
    print("\n" + "=" * 78)
    print("Frame loop FUN_{:08X}  (size={})".format(caller_ep, int(f.getBody().getNumAddresses())))
    print("=" * 78)
    # Walk instructions in order, collect call sites with their target.
    world_seen = False
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            if not r.getReferenceType().isCall(): continue
            tgt = fm.getFunctionContaining(r.getToAddress())
            tgt_ep = tgt.getEntryPoint().getOffset() if tgt else r.getToAddress().getOffset()
            is_world = (tgt_ep == WORLD)
            if is_world: world_seen = True
            reads, dev = cam_reads_and_device(tgt_ep) if tgt else (set(), False)
            phase = "AFTER " if (world_seen and not is_world) else ("WORLD>" if is_world else "before")
            extra = ""
            if dev: extra += " [DRAWS]"
            if reads: extra += " reads={}".format(sorted(reads))
            tag = " <== WORLD DISPATCHER" if is_world else ""
            if phase == "AFTER " and (dev or reads):
                print("  {} @{}  -> FUN_{:08X}{}{}".format(
                    phase, ins.getAddress(), tgt_ep, extra, tag))
            elif is_world:
                print("  {} @{}  -> FUN_{:08X}{}".format(phase, ins.getAddress(), tgt_ep, tag))

print("\nDone.")
