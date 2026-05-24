# Trace shadow render. FUN_0081FAA0 owns the human_shadow.raw texture.
# Find what calls it / what uses the loaded texture and where the
# projection happens.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Search where "CastHumanShadow" string is referenced.
print("[1] Refs to 'CastHumanShadow' (0x00C07168):")
print("-" * 78)
for r in ref.getReferencesTo(addr(0x00C07168)):
    fr = r.getFromAddress()
    f = af.getFunctionContaining(fr)
    if f:
        print("  {} in FUN_{:08X}  size={}".format(
            fr, f.getEntryPoint().getOffset(),
            int(f.getBody().getNumAddresses())))

# Search where "human_shadow.raw" texture loader is used.
print("")
print("[2] Callers of FUN_0081FAA0 (human_shadow.raw loader):")
print("-" * 78)
for r in ref.getReferencesTo(addr(0x0081FAA0)):
    if not r.getReferenceType().isCall(): continue
    fr = r.getFromAddress()
    f = af.getFunctionContaining(fr)
    if f:
        print("  {} in FUN_{:08X}  size={}".format(
            fr, f.getEntryPoint().getOffset(),
            int(f.getBody().getNumAddresses())))

# Decompile FUN_0081FAA0.
print("")
print("[3] Decompile FUN_0081FAA0:")
print("-" * 78)
di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
f = af.getFunctionAt(addr(0x0081FAA0))
if f:
    res = di.decompileFunction(f, 60, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC())

# Decompile FUN_008237B0 (ShadowsOnObjects / CloudShadows config).
print("")
print("[4] Decompile FUN_008237B0:")
print("-" * 78)
f = af.getFunctionAt(addr(0x008237B0))
if f:
    res = di.decompileFunction(f, 60, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC()[:3000])
