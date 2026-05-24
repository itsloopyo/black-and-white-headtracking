from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
mon = ConsoleTaskMonitor()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

dec = DecompInterface()
dec.openProgram(currentProgram)

def dump(ep, max_lines=120):
    f = af.getFunctionAt(addr(ep))
    print("=" * 78)
    print("FUN_{:08X}".format(ep))
    print("=" * 78)
    r = dec.decompileFunction(f, 180, mon)
    if not r or not r.getDecompiledFunction():
        print("  decompile failed")
        return
    src = r.getDecompiledFunction().getC()
    lines = src.splitlines()
    for ln in lines[:max_lines]:
        print(ln)
    if len(lines) > max_lines:
        print("  ... ({} more lines)".format(len(lines) - max_lines))

for ep in [0x0083A1D0, 0x00839F10]:
    dump(ep, 80)
    print("")
