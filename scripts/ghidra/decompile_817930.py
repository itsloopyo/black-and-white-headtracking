# Print decompilation of FUN_00817930 + a few key callees to understand
# whether it produces visible pre-transformed verts (would mean hooking it
# breaks character head-tracking) or just internal state for the shadow.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
mon = ConsoleTaskMonitor()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

dec = DecompInterface()
dec.openProgram(currentProgram)

def dump(ep, lines=200):
    f = af.getFunctionAt(addr(ep))
    if not f:
        print("# no function at 0x{:08X}".format(ep))
        return
    print("=" * 78)
    print("FUN_{:08X}  size={}  signature={}".format(
        ep, f.getBody().getNumAddresses(), f.getPrototypeString(False, False)))
    print("=" * 78)
    r = dec.decompileFunction(f, 180, mon)
    if not r or not r.getDecompiledFunction():
        print("# decompile failed")
        return
    src = r.getDecompiledFunction().getC()
    for i, ln in enumerate(src.splitlines()):
        if i > lines:
            print("  ... ({} more lines)".format(len(src.splitlines()) - lines))
            break
        print(ln)

dump(0x00817930, 240)
