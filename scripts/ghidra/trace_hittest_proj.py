# Decide whether the collar hit-test re-projects via the billboard projector
# (clean, update phase) or reads a cached rotated screen rect. Decompile:
#  - FUN_00536110 (hit-test geometry helper called by FUN_005362e0)
#  - FUN_008404a0 (billboard create - shows the struct fields used)
#  - 0x00405170 (collar vtable+0x74 hit-test thunk) via disasm
#  - callers of FUN_00840530 (who projects billboards, and in which phase)

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
lst  = currentProgram.getListing()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def show(entry, label):
    f = af.getFunctionContaining(addr(entry))
    if not f: print("{} 0x{:08X}: no func".format(label, entry)); return
    sz = int(f.getBody().getNumAddresses())
    print("=" * 78); print("=== {} {} 0x{:08X} size={}".format(label, f.getName(), f.getEntryPoint().getOffset(), sz)); print("=" * 78)
    if sz > 2200: print("(large; skipping)"); return
    res = di.decompileFunction(f, 60, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")

show(0x00536110, "hit-test geom helper")
show(0x005edc10, "hit handler FUN_005edc10")

print("\n=== disasm 0x00405170 (vtable+0x74 hit-test) ===")
a = addr(0x00405170)
for _ in range(16):
    ins = lst.getInstructionAt(a)
    if ins is None: print("  (no instr)"); break
    print("  0x{:08X}: {}".format(a.getOffset(), ins.toString()))
    a = a.add(ins.getLength())

print("\n=== callers of billboard projector FUN_00840530 ===")
for r in ref.getReferencesTo(addr(0x00840530)):
    f = af.getFunctionContaining(r.getFromAddress())
    print("  0x{:08X} in {}".format(r.getFromAddress().getOffset(), f.getName() if f else "?"))
