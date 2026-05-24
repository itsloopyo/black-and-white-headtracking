# Decompile FUN_0081b370 (screen-point -> world-ray) and the ground-intersect
# helper FUN_00802680, to see exactly how the basis (g_cameraStruct) and eye
# (g_cameraPivot) build the ray and where it intersects, so we inject the right
# matrix for cursor picks.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

for t in (0x0081B370, 0x00802680):
    f = af.getFunctionContaining(addr(t))
    if not f:
        print("0x{:08X}: no function".format(t)); continue
    print("=" * 78)
    print("=== {} entry=0x{:08X} size={}".format(
        f.getName(), f.getEntryPoint().getOffset(),
        int(f.getBody().getNumAddresses())))
    print("=" * 78)
    res = di.decompileFunction(f, 90, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC())
    else:
        print("(decompile failed)")
