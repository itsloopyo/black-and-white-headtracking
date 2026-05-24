# Decide which matrix the GUI pick projection uses. FUN_00519960 calls
# FUN_00819390 / FUN_008190d0 to get the screen position and FUN_007fb3f0 with a
# copy of g_cameraStruct. Whichever reads *DAT_00ea9ea0 (=g_scaledMatrix, rotated)
# vs g_cameraStruct (clean) determines the fix.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def show(entry):
    f = af.getFunctionContaining(addr(entry))
    if not f: print("0x{:08X}: no func".format(entry)); return
    sz = int(f.getBody().getNumAddresses())
    print("=" * 78); print("=== {} 0x{:08X} size={}".format(f.getName(), entry, sz)); print("=" * 78)
    if sz > 1800: print("(large; skipping)"); return
    res = di.decompileFunction(f, 60, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")
for e in (0x00819390, 0x008190d0, 0x007fb3f0):
    show(e)

# Who calls FUN_00519960 (the pick)? Confirm it ties to the citadel/leash widget.
print("\n=== callers of FUN_00519960 (pick) ===")
for r in ref.getReferencesTo(addr(0x00519960)):
    f = af.getFunctionContaining(r.getFromAddress())
    print("  0x{:08X} in {}".format(r.getFromAddress().getOffset(), f.getName() if f else "?"))
