# Confirm which function computes the authoritative cursor world point that
# collar selection consumes. Decompile FUN_005e5620's callers and the helpers
# it calls, plus FUN_00800c30 / FUN_00802550.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def show(a, label):
    f = af.getFunctionContaining(addr(a))
    if not f:
        print("=== {} 0x{:08X}: no function".format(label, a)); return
    print("=" * 78)
    print("=== {} {}  entry=0x{:08X}  size={}".format(
        label, f.getName(), f.getEntryPoint().getOffset(),
        int(f.getBody().getNumAddresses())))
    res = di.decompileFunction(f, 60, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC())
    else:
        print("(decompile failed)")

# Callers of FUN_005e5620 (the cursor->world candidate).
print("### CALLERS of FUN_005e5620 ###")
for r in ref.getReferencesTo(addr(0x005E5620)):
    fa = r.getFromAddress()
    cf = af.getFunctionContaining(fa)
    print("  call from 0x{:08X} in {}".format(
        fa.getOffset(), cf.getName() if cf else "?"))

# Helpers used to build / store the cursor world point.
show(0x00802550, "helper")
show(0x0054C180, "store?")
