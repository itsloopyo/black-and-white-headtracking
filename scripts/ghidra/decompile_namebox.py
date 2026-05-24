# Dump decompiled C for the name-box setup and the screen->world helper.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

TARGETS = [
    0x008334C0,  # name-box setup (contains the two S2W calls at 0x0083372E / 0x0083390B)
    0x0081B370,  # screen-point -> world ray/point (reads g_cameraStruct)
]

fact = currentProgram.getAddressFactory()
af   = currentProgram.getFunctionManager()

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
    res = di.decompileFunction(f, 120, mon)
    if res is None or not res.decompileCompleted():
        print("(decompile failed)")
        continue
    print(res.getDecompiledFunction().getC())
    print("")
