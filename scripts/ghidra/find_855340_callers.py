# Find callers of FUN_00855340 (billboard-basis builder) and disassemble the
# ~8 instructions before each call site so we can see which matrix pointer is
# pushed as its argument (PUSH offset / LEA / MOV before the CALL). If it's
# g_scaledMatrix (0xEA9E40) the basis already tracks our rotation; if it's
# g_cameraStruct (0xEA1D28) or a separate clean matrix, that's the drift root.

fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
fm = currentProgram.getFunctionManager()
ref = currentProgram.getReferenceManager()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

TARGET = 0x00855340
NAMES = {
    0x00EA1D28: 'g_cameraStruct', 0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix', 0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot', 0x00EA9E90: 'point_EA9E90',
}

sites = []
for r in ref.getReferencesTo(addr(TARGET)):
    if r.getReferenceType().isCall():
        sites.append(r.getFromAddress())

print("Call sites of FUN_{:08X}: {}".format(TARGET, len(sites)))
for site in sites:
    cont = fm.getFunctionContaining(site)
    cn = "FUN_{:08X}".format(cont.getEntryPoint().getOffset()) if cont else "?"
    print("\n" + "=" * 78)
    print("call @ {} in {}".format(site, cn))
    print("=" * 78)
    # back up ~10 instructions
    ins = listing.getInstructionAt(site)
    seq = []
    p = ins
    for _ in range(12):
        p = p.getPrevious()
        if not p: break
        seq.append(p)
    for p in reversed(seq):
        annot = ""
        for op in range(p.getNumOperands()):
            for rr in p.getOperandReferences(op):
                t = rr.getToAddress().getOffset()
                if t in NAMES: annot = "  <-- " + NAMES[t]
        print("  {}  {}{}".format(p.getAddress(), p, annot))
    print("  {}  {}   <== CALL".format(ins.getAddress(), ins))

print("\nDone.")
