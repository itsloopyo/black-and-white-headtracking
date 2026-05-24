# FUN_005E42E0: 3608 bytes, in-world-render. Reads kCursorX + calls FUN_0081B370
# (screen-to-world) at 0x005E44BE; writes cursor-snapshot globals at adjacent
# addresses. Strongly suggests this is the per-frame "cast cursor ray into the
# world to determine what is under the cursor" function. If creature + scroll
# mouseover use the result of this raycast, it's the fix surface.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram); mon = ConsoleTaskMonitor()

EP = 0x005E42E0

NAMES = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
    0x00E852C0: 'kCursorX',
    0x00E852C4: 'kCursorY',
    0x00EA1B10: 'cursorSnapX',
    0x00EA1B14: 'cursorSnapY',
    0x00EA1B18: 'maybe_world_x',  # often the world point follows the snapshot
    0x00EA1B1C: 'maybe_world_y',
    0x00EA1B20: 'maybe_world_z',
}

print("=== Callers of FUN_{:08X} ===".format(EP))
for r in ref.getReferencesTo(addr(EP)):
    if not r.getReferenceType().isCall(): continue
    f = af.getFunctionContaining(r.getFromAddress())
    print("  call@0x{:08X}  in {} 0x{:08X}".format(
        r.getFromAddress().getOffset(),
        f.getName() if f else "?",
        f.getEntryPoint().getOffset() if f else 0))

f = af.getFunctionAt(addr(EP))
print("\nSize: {} bytes".format(int(f.getBody().getNumAddresses())))

print("\n=== Annotated global references ===")
for a in f.getBody().getAddresses(True):
    ins = listing.getInstructionAt(a)
    if not ins: continue
    for r in ins.getReferencesFrom():
        t = r.getToAddress().getOffset()
        if t in NAMES:
            print("  0x{:08X}  {:20s}  {}".format(a.getOffset(), NAMES[t], str(ins)))

print("\n=== Function calls (count) ===")
calls = {}
for a in f.getBody().getAddresses(True):
    ins = listing.getInstructionAt(a)
    if not ins: continue
    for r in ins.getReferencesFrom():
        if not r.getReferenceType().isCall(): continue
        t = r.getToAddress().getOffset()
        calls[t] = calls.get(t, 0) + 1
for t, n in sorted(calls.items(), key=lambda x: -x[1])[:25]:
    print("  {:5d}x  FUN_{:08X}".format(n, t))

print("\n=== Decompile (long timeout) ===")
res = di.decompileFunction(f, 600, mon)
if res and res.decompileCompleted():
    c = res.getDecompiledFunction().getC()
    print(c[:35000])
    if len(c) > 35000:
        print("... (truncated; full len {})".format(len(c)))
else:
    print("(decompile failed)")
    err = res.getErrorMessage() if res else "no result"
    print("error:", err)

print("\n[done]")
