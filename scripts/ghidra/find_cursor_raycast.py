# All callers of FUN_0081b370 (screen->world ray helper). Then check
# which are reachable from FUN_0054DA80, i.e. INSIDE our sandwich.
# Those callers are seeing rotated g_cameraStruct.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

reach = set()
stack = [0x0054DA80]
while stack:
    ep = stack.pop()
    if ep in reach: continue
    reach.add(ep)
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    for c in f.getCalledFunctions(ConsoleTaskMonitor()):
        if c: stack.append(c.getEntryPoint().getOffset())

print("All callers of FUN_0081b370 (screen->world ray):")
print("=" * 78)
callers = {}
for r in ref.getReferencesTo(addr(0x0081b370)):
    if not r.getReferenceType().isCall(): continue
    f = af.getFunctionContaining(r.getFromAddress())
    if f:
        ep = f.getEntryPoint().getOffset()
        callers[ep] = callers.get(ep, 0) + 1

for ep in sorted(callers):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    nm = f.getName() if f else '?'
    inscope = '[IN sandwich]' if ep in reach else '[OUT]'
    print("  FUN_{:08X} {} size={:5d} calls={} {}".format(
        ep, nm, sz, callers[ep], inscope))

# Also dump FUN_0081b370's signature for confirmation.
print("")
print("Decompile head of FUN_0081b370:")
print("-" * 78)
di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
f = af.getFunctionAt(addr(0x0081b370))
res = di.decompileFunction(f, 30, mon)
if res and res.decompileCompleted():
    print(res.getDecompiledFunction().getC()[:1500])
