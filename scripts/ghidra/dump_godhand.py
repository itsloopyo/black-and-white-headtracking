# Focused: decompile FUN_005bc0a0 (suspected god-hand cursor->world) and list
# its callers, to confirm it reads the cursor coord, casts the ray, and where
# the resulting world point goes (render vs interaction).

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

target = 0x005BC0A0
print("### callers of FUN_{:08X} ###".format(target))
for r in ref.getReferencesTo(addr(target)):
    if not r.getReferenceType().isCall(): continue
    f = af.getFunctionContaining(r.getFromAddress())
    print("  call@0x{:08X} in {}".format(
        r.getFromAddress().getOffset(), f.getName() if f else "?"))

print("")
f = af.getFunctionAt(addr(target))
try:
    res = di.decompileFunction(f, 120, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted()
          else "(decompile failed)")
except Exception as e:
    print("decompile exception:", e)
