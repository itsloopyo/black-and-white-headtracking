from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
for entry in (0x00464E20, 0x004651D0, 0x004684F0, 0x004686B0, 0x00468940, 0x00465C70):
    f = af.getFunctionContaining(addr(entry))
    print("=" * 78)
    print("=== {} 0x{:08X} size={}".format(f.getName(), entry, int(f.getBody().getNumAddresses())))
    print("=" * 78)
    res = di.decompileFunction(f, 90, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")
