# List every global memory address WRITTEN by the camera builders
# FUN_00819920 and FUN_00819F50. We hook these at their tail and already
# rebuild g_scaledMatrix/g_mirrorMatrix/g_invScaledMatrix from a head-rotated
# camera. If the billboard/sprite camera globals (seen in FUN_00854630:
# 0xEA1CC8 matrix, 0xEA9EA0, 0xF03140 basis, 0xEA1B08 angle) are also written
# here, they are camera-derived state we must rotate too, or every billboard
# (name labels, particles, markers) drifts against the rotated world.

fm   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

BUILDERS = [0x00819920, 0x00819F50]

# Region labels for known camera globals.
def label(off):
    known = {
        0x00EA1D28: 'g_cameraStruct[0]',
        0x00EA9E40: 'g_scaledMatrix[0]',
        0x00EA1D58: 'g_mirrorMatrix[0]',
        0x00EA9DE0: 'g_invScaledMatrix[0]',
        0x00EA1DB8: 'g_cameraPivot[0]',
        0x00EA1DC4: 'g_cameraTarget[0]',
    }
    # range tags
    if 0x00EA1CC8 <= off <= 0x00EA1CF4: return 'BILLBOARD_MAT_EA1CC8 +{:#x}'.format(off-0x00EA1CC8)
    if 0x00F03140 <= off <= 0x00F0314C: return 'BILLBOARD_BASIS_F03140 +{:#x}'.format(off-0x00F03140)
    if 0x00EA9EA0 <= off <= 0x00EA9ED0: return 'MAT_EA9EA0 +{:#x}'.format(off-0x00EA9EA0)
    if 0x00EA1B00 <= off <= 0x00EA1B10: return 'ANGLE_EA1B08 region'
    if off in known: return known[off]
    if 0x00EA1D28 <= off <= 0x00EA1D54: return 'g_cameraStruct +{:#x}'.format(off-0x00EA1D28)
    if 0x00EA9E40 <= off <= 0x00EA9E6C: return 'g_scaledMatrix +{:#x}'.format(off-0x00EA9E40)
    if 0x00EA1D58 <= off <= 0x00EA1D84: return 'g_mirrorMatrix +{:#x}'.format(off-0x00EA1D58)
    if 0x00EA9DE0 <= off <= 0x00EA9E0C: return 'g_invScaledMatrix +{:#x}'.format(off-0x00EA9DE0)
    return ''

for b in BUILDERS:
    f = fm.getFunctionAt(addr(b))
    print("=" * 78)
    print("Writes by FUN_{:08X} (size={})".format(b, int(f.getBody().getNumAddresses())))
    print("=" * 78)
    writes = {}
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            rt = r.getReferenceType()
            if rt.isWrite():
                t = r.getToAddress()
                if t.isMemoryAddress():
                    off = t.getOffset()
                    writes.setdefault(off, 0)
                    writes[off] += 1
    for off in sorted(writes):
        lab = label(off)
        # only show data-segment-ish addresses (globals), skip stack
        if off < 0x00400000: continue
        print("  0x{:08X}  x{:<3} {}".format(off, writes[off], lab))
    print("")

print("Done.")
