# Decompile the candidate scroll-pick functions: all snapshot readers in the
# 0x00855xxx-0x00858xxx range, FUN_00868C80, FUN_007AC640. Also decompile
# FUN_00809E50 to see whether/when it overwrites DAT_00EA1AC8 (which would
# clobber our cursor shift if it runs between Hook_PickOrchestrator and the
# scroll pick).

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
    0x00EA1AC8: 'SNAP_X',
    0x00EA1ACC: 'SNAP_Y',
    0x00EA1B10: 'cursorSnap2X',
    0x00EA1B14: 'cursorSnap2Y',
}
FUNCS = {
    0x00519960: 'FUN_objPick',
    0x0081B370: 'FUN_s2w',
    0x00819390: 'FUN_proj',
    0x008190d0: 'FUN_proj_b',
    0x005E42E0: 'FUN_pickOrch',
    0x00855340: 'FUN_855340',
    0x00855040: 'FUN_855040',
}

def annotate(ep):
    f = af.getFunctionAt(addr(ep))
    if not f: return
    body = f.getBody()
    hits = []
    for a in body.getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            t = r.getToAddress().getOffset()
            if t in NAMES:
                hits.append((a.getOffset(), NAMES[t], str(ins)))
    print("  global references:")
    for ad, n, s in hits:
        print("    0x{:08X}  {:18s}  {}".format(ad, n, s))

def show(ep, label, maxlen=12000):
    f = af.getFunctionAt(addr(ep))
    if not f:
        print("(no function at {:08X})".format(ep)); return
    sz = int(f.getBody().getNumAddresses())
    print("")
    print("#" * 78)
    print("### {}  FUN_{:08X}  size={} ###".format(label, ep, sz))
    print("#" * 78)
    annotate(ep)
    if sz > 4000:
        print("(too large; refs only)")
        return
    res = di.decompileFunction(f, 120, mon)
    if res and res.decompileCompleted():
        c = res.getDecompiledFunction().getC()
        print(c[:maxlen])
        if len(c) > maxlen:
            print("... (truncated; full len {})".format(len(c)))
    else:
        print("(decompile failed)")
        err = res.getErrorMessage() if res else "no result"
        print("error:", err)

# Order: easiest/most-likely first.
show(0x00868C80, "snap reader: FUN_00868C80")
show(0x00856A00, "snap reader: FUN_00856A00 (smallest in 855/856 cluster)")
show(0x00856B30, "snap reader: FUN_00856B30")
show(0x008587B0, "snap reader: FUN_008587B0")
show(0x008589B0, "snap reader: FUN_008589B0")
show(0x007AC640, "snap reader: FUN_007AC640")

# Show callers of the biggest snap readers - those tell us what subsystem.
print("\n=== Callers of FUN_00855440 (2089 bytes, snap reader) ===")
for r in ref.getReferencesTo(addr(0x00855440)):
    if not r.getReferenceType().isCall(): continue
    f = af.getFunctionContaining(r.getFromAddress())
    print("  call@0x{:08X} from {}".format(
        r.getFromAddress().getOffset(),
        f.getName() if f else "?"))

print("\n=== Callers of FUN_00855C70 (1369 bytes, snap reader) ===")
for r in ref.getReferencesTo(addr(0x00855C70)):
    if not r.getReferenceType().isCall(): continue
    f = af.getFunctionContaining(r.getFromAddress())
    print("  call@0x{:08X} from {}".format(
        r.getFromAddress().getOffset(),
        f.getName() if f else "?"))

print("\n[done]")
