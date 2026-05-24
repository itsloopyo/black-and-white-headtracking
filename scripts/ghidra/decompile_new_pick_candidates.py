# Decompile the new pick-path candidates revealed by find_pick_callers.py.
# Focus: FUN_00519AD0 (3382 bytes, IN-WORLD-RENDER, calls FUN_00519960) and
# FUN_00542A90 (439 bytes, reads cursor + calls S2W). Also decompile the small
# IN-WORLD-RENDER ones FUN_00682F30 and FUN_005BD2A0 to map the full set.
#
# For each: list which engine globals it references (cursor, camera matrices,
# projection helpers) so we can see at a glance what data feeds the pick.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram); mon = ConsoleTaskMonitor()

NAMES = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
    0x00E852C0: 'kCursorX',
    0x00E852C4: 'kCursorY',
    0x00E839F0: 'kScreenHalfX',
    0x00E839F4: 'kScreenHalfY',
    0x00E839E0: 'kViewForward',
    0x00C3812C: 'kProjScaleX',
    0x00C38130: 'kProjScaleY',
    0x00E83A00: 'kScaleX',
    0x00E83A04: 'kScaleY',
    0x00ea1ac8: 'DAT_ea1ac8',  # mystery cursor-like global from old dig
    0x00ea1acc: 'DAT_ea1acc',
    0x00519960: 'FUN_objPick',
    0x0081B370: 'FUN_s2w',
    0x00819390: 'FUN_proj',
    0x00840530: 'FUN_billboardDraw',
}

def annotate(ep):
    f = af.getFunctionAt(addr(ep))
    if not f:
        print("  (no function at 0x{:08X})".format(ep)); return
    body = f.getBody()
    hits = []
    for a in body.getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            t = r.getToAddress().getOffset()
            if t in NAMES:
                hits.append((a.getOffset(), NAMES[t], str(ins)))
    print("  references:")
    for ad, n, s in hits:
        print("    0x{:08X}  {:20s}  {}".format(ad, n, s))

def show(ep, label, maxlen=15000):
    print("")
    print("#" * 78)
    print("### {}  FUN_{:08X} ###".format(label, ep))
    print("#" * 78)
    f = af.getFunctionAt(addr(ep))
    if not f:
        print("(no function)"); return
    sz = int(f.getBody().getNumAddresses())
    print("size={} bytes".format(sz))
    annotate(ep)
    if sz > 5000:
        print("(too large; refs only)")
        return
    res = di.decompileFunction(f, 90, mon)
    if res and res.decompileCompleted():
        c = res.getDecompiledFunction().getC()
        print(c[:maxlen])
        if len(c) > maxlen:
            print("... (truncated; full len {})".format(len(c)))
    else:
        print("(decompile failed)")

# Primary candidates
show(0x00519AD0, "PRIMARY: in-world cursor-over object pick (3382 bytes)")
show(0x00542A90, "candidate: cursor + S2W (439 bytes)")
show(0x00682F30, "small in-world pick caller (111 bytes)")
show(0x005BD2A0, "S2W caller (232 bytes)")

print("\n[done]")
