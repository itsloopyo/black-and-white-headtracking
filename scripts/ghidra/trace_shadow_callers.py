# Find callers of FUN_0081FFF0 (shadow renderer). Look for a parent
# function that's called per-frame and is the right granularity to
# sandwich (covers shadows + maybe more).

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def callers_of(tgt, max_depth=4):
    seen = set()
    def walk(t, depth):
        if depth > max_depth: return
        if t in seen: return
        seen.add(t)
        for r in ref.getReferencesTo(addr(t)):
            if not r.getReferenceType().isCall(): continue
            f = af.getFunctionContaining(r.getFromAddress())
            if not f: continue
            ep = f.getEntryPoint().getOffset()
            sz = int(f.getBody().getNumAddresses())
            print("  {}-> FUN_{:08X}  size={}".format('  ' * depth, ep, sz))
            walk(ep, depth + 1)
    walk(tgt, 0)

print("Caller tree up from FUN_0081FFF0 (shadow renderer):")
print("=" * 78)
callers_of(0x0081FFF0)

print("")
print("Caller tree up from FUN_0081FE10 (small shadow helper):")
print("=" * 78)
callers_of(0x0081FE10)

# Decompile head of FUN_0081FFF0.
print("")
print("Head of FUN_0081FFF0:")
print("=" * 78)
di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
f = af.getFunctionAt(addr(0x0081FFF0))
res = di.decompileFunction(f, 60, mon)
if res and res.decompileCompleted():
    print(res.getDecompiledFunction().getC()[:4500])
