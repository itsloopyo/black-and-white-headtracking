from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
mon = ConsoleTaskMonitor()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

dec = DecompInterface()
dec.openProgram(currentProgram)

def dump_decompile(ep):
    f = af.getFunctionAt(addr(ep))
    print("=" * 78)
    print("FUN_{:08X}  signature={}".format(ep, f.getPrototypeString(False, False)))
    print("=" * 78)
    r = dec.decompileFunction(f, 180, mon)
    if r and r.getDecompiledFunction():
        print(r.getDecompiledFunction().getC())

def dump_prologue(ep, n=24):
    print("--- prologue for FUN_{:08X} ---".format(ep))
    cur = listing.getInstructionAt(addr(ep))
    for _ in range(n):
        if cur is None: break
        print("  {}  {}".format(cur.getAddress(), cur))
        cur = cur.getNext()

dump_prologue(0x007faff0, 30)
print("")
dump_decompile(0x007faff0)
