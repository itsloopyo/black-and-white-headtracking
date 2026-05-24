# Decompile the GUI-range g_cameraStruct readers. One projects a world anchor to
# a widget's 2D screen position (used by the screen-space hit-test FUN_00536110).
# Looking for: reads g_cameraStruct, divides by view-Z, multiplies by screen
# scale (DAT_00e83a00/04 or DAT_00e839f0/f4), writes a 2D position.

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
    if sz > 1500: print("(large; skipping)"); return
    res = di.decompileFunction(f, 60, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")

for e in (0x00519960, 0x0045a960, 0x00575ea0, 0x005bdda0, 0x005cbf10, 0x005d56c0, 0x004270d0):
    show(e)
