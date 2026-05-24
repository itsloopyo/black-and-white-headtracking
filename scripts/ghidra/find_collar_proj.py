# The per-collar screen positions (orbiting points ~1897-1903) that the GUI
# hit-test compares the cursor against are projected by some citadel-range
# caller of FUN_00819390 - NOT FUN_00466730 (that one only projects the citadel
# centre). Decompile the citadel/leash-range callers to find the one that
# iterates the 3 collar objects and projects each.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def show(entry):
    f = af.getFunctionContaining(addr(entry))
    if not f: print("0x{:08X}: no func".format(entry)); return
    sz = int(f.getBody().getNumAddresses())
    print("=" * 78); print("=== {} 0x{:08X} size={}".format(f.getName(), entry, sz)); print("=" * 78)
    if sz > 2200: print("(large; skipping body)"); return
    res = di.decompileFunction(f, 60, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")
# Citadel-range / leash-selector candidates that call FUN_00819390 or iterate collars.
for e in (0x0046a680, 0x006483d0, 0x004670d0, 0x00467360):
    show(e)
