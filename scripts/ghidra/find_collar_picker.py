# The three collar meshes live in DAT_00c5e3d8 / dc / e0. Find every function
# that reads them - that set contains the selector's draw + pick. Decompile the
# most promising (the one that also reads the mouse / does a screen projection).

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

funcs = {}
for g in (0x00C5E3D8, 0x00C5E3DC, 0x00C5E3E0):
    for r in ref.getReferencesTo(addr(g)):
        fa = r.getFromAddress()
        f = af.getFunctionContaining(fa)
        if f:
            funcs.setdefault(f.getEntryPoint().getOffset(), f.getName())
            print("  0x{:08X} reads 0x{:08X}  in {}".format(fa.getOffset(), g, f.getName()))

print("\n=== decompiling readers ===")
for entry, name in sorted(funcs.items()):
    f = af.getFunctionContaining(addr(entry))
    print("=" * 78)
    print("=== {} entry=0x{:08X} size={}".format(name, entry, int(f.getBody().getNumAddresses())))
    print("=" * 78)
    res = di.decompileFunction(f, 90, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")
