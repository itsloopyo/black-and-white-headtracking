# Trace the collar GUI hit-test chain to the world->screen projector.
# Input dispatch FUN_00570350 calls vtable+0x74 (hit test) then FUN_005362e0 /
# FUN_005f2890. Read the collar vtable (0x008c84a0) hit-test slot and decompile
# the chain. Also enumerate every function reading g_cameraStruct (clean,
# 0x00EA1D28) so the projector that drives the hit-test stands out.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
mem  = currentProgram.getMemory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
def rd32(a): return mem.getInt(addr(a)) & 0xffffffff
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def show(entry, label):
    f = af.getFunctionContaining(addr(entry))
    if not f: print("{} 0x{:08X}: no func".format(label, entry)); return
    sz = int(f.getBody().getNumAddresses())
    print("=" * 78); print("=== {} {} 0x{:08X} size={}".format(label, f.getName(), f.getEntryPoint().getOffset(), sz)); print("=" * 78)
    if sz > 2000: print("(large; skipping)"); return
    res = di.decompileFunction(f, 60, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")

vt = 0x008c84a0
print("vtable+0x74 (hit-test) = 0x{:08X}".format(rd32(vt + 0x74)))
print("vtable+0x690 = 0x{:08X}  vtable+0x694 = 0x{:08X}".format(rd32(vt+0x690), rd32(vt+0x694)))
show(rd32(vt + 0x74), "hit-test vtable+0x74")
show(0x00570350, "input dispatch")
show(0x005362e0, "handler A")
show(0x005f2890, "handler B")

# Enumerate g_cameraStruct (clean) readers - the buggy projector is among GUI ones.
CS = 0x00EA1D28
print("\n=== g_cameraStruct (clean 0x00EA1D28) readers ===")
seen=set()
for off in range(0, 0x30, 4):
    for r in ref.getReferencesTo(addr(CS+off)):
        f = af.getFunctionContaining(r.getFromAddress())
        if f and f.getEntryPoint().getOffset() not in seen:
            seen.add(f.getEntryPoint().getOffset())
            print("  0x{:08X} {}".format(f.getEntryPoint().getOffset(), f.getName()))
