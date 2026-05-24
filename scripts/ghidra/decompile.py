# Dump decompiled C for a list of function addresses.
#
# Edit TARGETS below or pass via script args.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

TARGETS = [
    0x0081C090,  # final submit at end of shadow renderer
    0x004427B0,  # called per-vertex with xy
    0x00442700,  # called with 3 floats
    0x0081FE50,  # helper at start/middle of shadow loop
]

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

for t in TARGETS:
    f = af.getFunctionAt(addr(t))
    if not f:
        print("=== 0x{:08X}: no function".format(t))
        continue
    print("=" * 78)
    print("=== FUN_{:08X}  name={}  size={}".format(
        t, f.getName(), int(f.getBody().getNumAddresses())))
    print("=" * 78)
    res = di.decompileFunction(f, 60, mon)
    if res is None or not res.decompileCompleted():
        print("(decompile failed)")
        continue
    print(res.getDecompiledFunction().getC())
    print("")
