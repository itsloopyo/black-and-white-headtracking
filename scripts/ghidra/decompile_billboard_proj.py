# Decompile the billboard projector FUN_00840530 (reads g_cameraStruct) and the
# billboard creator FUN_008404a0, plus the hit-test geometry helper FUN_00536110
# used by FUN_005362e0. Goal: confirm whether the collar billboard projects via
# g_cameraStruct (clean in update phase, rotated in sandwich) so draw and pick
# diverge, and whether the hit-test re-projects through the same function.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def show(entry, label):
    f = af.getFunctionContaining(addr(entry))
    if not f: print("{} 0x{:08X}: no func".format(label, entry)); return
    sz = int(f.getBody().getNumAddresses())
    print("=" * 78); print("=== {} {} 0x{:08X} size={}".format(label, f.getName(), f.getEntryPoint().getOffset(), sz)); print("=" * 78)
    if sz > 2600: print("(large; skipping)"); return
    res = di.decompileFunction(f, 60, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")
show(0x00840530, "billboard projector?")
show(0x008404a0, "billboard create")
show(0x00536110, "hit-test geom helper")
