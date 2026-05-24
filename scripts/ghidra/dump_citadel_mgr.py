# Decompile the Citadel leash-selector manager (FUN_00464950, the sole caller of
# the collar constructor) and its neighbours in CitadelHeart.cpp, plus the GUI
# hit-test the input dispatch (FUN_00570350) routes through (vtable+0x74), to
# find where the collar's screen position / pick is computed against the cursor.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def show(entry, label, maxsz=2400):
    f = af.getFunctionContaining(addr(entry))
    if not f:
        print("{} 0x{:08X}: no func".format(label, entry)); return
    sz = int(f.getBody().getNumAddresses())
    print("=" * 78)
    print("=== {} {} 0x{:08X} size={}".format(label, f.getName(), f.getEntryPoint().getOffset(), sz))
    print("=" * 78)
    if sz > maxsz:
        print("(large; skipping)"); return
    res = di.decompileFunction(f, 60, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")

show(0x00464950, "selector mgr")
# CitadelHeart neighbours - functions immediately around the collar constructor.
for a in (0x00464760, 0x00464800, 0x004648a0):
    show(a, "neighbour")
