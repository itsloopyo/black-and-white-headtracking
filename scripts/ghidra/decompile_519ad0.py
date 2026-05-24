# Detailed analysis of FUN_00519AD0 - the prime candidate for B&W's per-frame
# object mouseover pick (loops world objects, projects each, picks best match
# under cursor). 3382 bytes, in-world-render, calls FUN_00519960 (object screen
# pick). If creature mouseover + scroll mouseover route through here, fixing
# this function fixes the broader pick-drift family.
#
# Strategy: longer decompile timeout, AND show every global / function reference
# the function makes so we don't have to wait for the full decompile to start
# reasoning. Also list its callers.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram); mon = ConsoleTaskMonitor()

EP = 0x00519AD0

NAMES = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
    0x00E852C0: 'kCursorX',
    0x00E852C4: 'kCursorY',
    0x00EA1B10: 'cursorSnapX',  # written from kCursorX in FUN_00542A90
    0x00EA1B14: 'cursorSnapY',
    0x00E839F0: 'kScreenHalfX',
    0x00E839F4: 'kScreenHalfY',
    0x00E839E0: 'kViewForward',
    0x00C3812C: 'kProjScaleX',
    0x00C38130: 'kProjScaleY',
    0x00E83A00: 'kScaleX',
    0x00E83A04: 'kScaleY',
}

FUNCS = {
    0x00519960: 'FUN_objPick',
    0x0081B370: 'FUN_s2w',
    0x00819390: 'FUN_proj',
    0x008190d0: 'FUN_proj_b',
    0x00819920: 'FUN_camBuild',
    0x00819F50: 'FUN_camBuild_b',
    0x00840530: 'FUN_billboardDraw',
    0x00466730: 'FUN_citadelGate',
    0x00542A90: 'FUN_cursorSnap',
}

print("=== Callers of FUN_{:08X} ===".format(EP))
for r in ref.getReferencesTo(addr(EP)):
    if not r.getReferenceType().isCall(): continue
    f = af.getFunctionContaining(r.getFromAddress())
    print("  call@0x{:08X} in {} 0x{:08X}".format(
        r.getFromAddress().getOffset(),
        f.getName() if f else "?",
        f.getEntryPoint().getOffset() if f else 0))

f = af.getFunctionAt(addr(EP))
print("\nSize: {} bytes".format(int(f.getBody().getNumAddresses())))

print("\n=== Global data references ===")
seen_globals = []
for a in f.getBody().getAddresses(True):
    ins = listing.getInstructionAt(a)
    if not ins: continue
    for r in ins.getReferencesFrom():
        t = r.getToAddress().getOffset()
        if t in NAMES:
            seen_globals.append((a.getOffset(), NAMES[t], str(ins)))
for ad, n, s in seen_globals:
    print("  0x{:08X}  {:20s}  {}".format(ad, n, s))

print("\n=== Function calls ===")
call_count = {}
for a in f.getBody().getAddresses(True):
    ins = listing.getInstructionAt(a)
    if not ins: continue
    for r in ins.getReferencesFrom():
        if not r.getReferenceType().isCall(): continue
        t = r.getToAddress().getOffset()
        label = FUNCS.get(t, "FUN_{:08X}".format(t))
        key = (t, label)
        call_count[key] = call_count.get(key, 0) + 1
for (t, label), n in sorted(call_count.items(), key=lambda x: -x[1]):
    print("  {:5d}x  {}  0x{:08X}".format(n, label, t))

print("\n=== Decompile (long timeout) ===")
res = di.decompileFunction(f, 600, mon)
if res and res.decompileCompleted():
    c = res.getDecompiledFunction().getC()
    print(c[:25000])
    if len(c) > 25000:
        print("... (truncated; full len {})".format(len(c)))
else:
    print("(decompile FAILED even at 600s)")
    err = res.getErrorMessage() if res else "no result"
    print("error:", err)

print("\n[done]")
