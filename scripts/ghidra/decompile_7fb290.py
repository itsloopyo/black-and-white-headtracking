from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
mon = ConsoleTaskMonitor()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

dec = DecompInterface()
dec.openProgram(currentProgram)

ep = 0x007FB290
f = af.getFunctionAt(addr(ep))
print("FUN_{:08X} signature: {}".format(ep, f.getPrototypeString(False, False)))
print("=" * 78)
print("Prologue:")
cur = listing.getInstructionAt(addr(ep))
for _ in range(30):
    if cur is None: break
    print("  {}  {}".format(cur.getAddress(), cur))
    cur = cur.getNext()

print()
print("Decompile:")
r = dec.decompileFunction(f, 180, mon)
if r and r.getDecompiledFunction():
    print(r.getDecompiledFunction().getC())
