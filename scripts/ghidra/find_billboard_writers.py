# Find the function(s) that WRITE the billboard camera globals that the
# sprite/billboard renderer FUN_00854630 projects through:
#   0xEA1CC8  4x3 billboard matrix (12 floats -> 0xEA1CF4)
#   0xF03140  3-float billboard basis vector
#   0xEA9EA0  matrix base used as DAT_00ea9ea0
#   0xEA1B08  billboard roll angle
# For each writer, report what camera globals it READS (so we know whether
# it derives them from g_cameraStruct/g_scaledMatrix and can be refreshed
# with head rotation like we refresh g_invScaledMatrix).

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

TARGETS = {
    0x00EA1CC8: 'billboard_mat',
    0x00EA1CCC: 'billboard_mat',
    0x00EA1CD0: 'billboard_mat',
    0x00F03140: 'billboard_basis',
    0x00F03144: 'billboard_basis',
    0x00F03148: 'billboard_basis',
    0x00EA9EA0: 'mat_ea9ea0',
    0x00EA1B08: 'billboard_angle',
}
CAMERA_STATE = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
    0x00EA1DC4: 'g_cameraTarget',
}

def cam_reads(fn):
    reads = set()
    for a in fn.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t = r.getToAddress().getOffset()
                if t in CAMERA_STATE: reads.add(CAMERA_STATE[t])
    return reads

writers = {}
for off, lab in TARGETS.items():
    for r in ref.getReferencesTo(addr(off)):
        if r.getReferenceType().isWrite():
            f = fm.getFunctionContaining(r.getFromAddress())
            if f:
                ep = f.getEntryPoint().getOffset()
                writers.setdefault(ep, set()).add(lab)

print("Writers of billboard camera globals:")
print("=" * 78)
for ep in sorted(writers):
    f = fm.getFunctionAt(addr(ep))
    reads = cam_reads(f)
    print("  FUN_{:08X} size={:<6} writes={} reads_camera={}".format(
        ep, int(f.getBody().getNumAddresses()),
        sorted(writers[ep]), sorted(reads) if reads else 'NONE'))

# Also: is FUN_00854630 (billboard renderer) reachable from world dispatcher?
print("")
print("Callers of FUN_00854630 (billboard renderer):")
for r in ref.getReferencesTo(addr(0x00854630)):
    rt = 'CALL' if r.getReferenceType().isCall() else str(r.getReferenceType())
    f = fm.getFunctionContaining(r.getFromAddress())
    where = "FUN_{:08X}".format(f.getEntryPoint().getOffset()) if f else "?"
    print("  {} from {} @ {}".format(rt, where, r.getFromAddress()))

print("\nDone.")
