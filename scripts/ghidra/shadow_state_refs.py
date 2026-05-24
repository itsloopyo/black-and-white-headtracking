# Inside FUN_0081FFF0, list every operand reference to camera-state
# globals so we can see WHERE in the function it touches them.

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

WATCH = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
    0x00EA1DC4: 'g_cameraTo',
    0x00EAA1A0: 'g_shadowProjVec',
}

# Walk every instruction in FUN_0081FFF0 and FUN_00815A70 and FUN_00817930
# noting operand refs into the WATCH set.
for tgt in [0x0081FFF0, 0x00815A70, 0x00817930]:
    f = af.getFunctionAt(addr(tgt))
    if not f: continue
    print("=" * 78)
    print("FUN_{:08X}:".format(tgt))
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t = r.getToAddress().getOffset()
                # match exact OR within 0x40 of a watched base (matrix offset)
                for base, label in WATCH.items():
                    if base <= t < base + 0x40:
                        delta = t - base
                        print("  {}  {}  -> {} + 0x{:X}".format(a, ins, label, delta))
                        break

print("")
print("Done.")
