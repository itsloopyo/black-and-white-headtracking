# Dump the Citadel leash-selector. The collar objects use vtable 0x008c84a0
# (built in FUN_00464650). Find: (a) the vtable methods, (b) callers of the
# constructor FUN_00464650 (the selector init / owning update loop), so we can
# locate the cursor->collar pick.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
mem  = currentProgram.getMemory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
def rd32(a):
    return mem.getInt(addr(a)) & 0xffffffff

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

# (a) vtable methods at 0x008c84a0 - read pointers until they stop looking like
# code addresses in .text (0x401000..0x8c0000).
print("=== vtable @ 0x008c84a0 ===")
vt = 0x008c84a0
methods = []
for i in range(40):
    p = rd32(vt + i*4)
    if p < 0x00401000 or p > 0x008c0000:
        break
    f = af.getFunctionContaining(addr(p))
    nm = f.getName() if f else "?"
    print("  [{:2}] 0x{:08X}  {}".format(i, p, nm))
    methods.append(p)

# (b) callers of the constructor FUN_00464650.
print("\n=== callers of FUN_00464650 (selector init) ===")
for r in ref.getReferencesTo(addr(0x00464650)):
    fa = r.getFromAddress()
    f = af.getFunctionContaining(fa)
    print("  0x{:08X} in {}".format(fa.getOffset(), f.getName() if f else "?"))

# Decompile each vtable method (small ones) to find the pick / mouse-over test.
print("\n=== vtable method decompiles ===")
seen = set()
for p in methods:
    f = af.getFunctionContaining(addr(p))
    if not f or f.getEntryPoint().getOffset() in seen:
        continue
    seen.add(f.getEntryPoint().getOffset())
    sz = int(f.getBody().getNumAddresses())
    print("=" * 78)
    print("=== {} 0x{:08X} size={}".format(f.getName(), f.getEntryPoint().getOffset(), sz))
    print("=" * 78)
    if sz > 1600:
        print("(large; skipping body)")
        continue
    res = di.decompileFunction(f, 60, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")
