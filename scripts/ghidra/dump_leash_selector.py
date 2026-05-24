# Locate the GLeashSelector class vtable via RTTI and dump its virtual methods,
# then decompile the ones most likely to hold the cursor->collar pick (update /
# click / draw). We are looking for a forward projection of each collar's world
# position to screen compared against the mouse - that projection uses the clean
# camera and needs to use the rotated render matrix instead.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
lst  = currentProgram.getListing()
mem  = currentProgram.getMemory()
st   = currentProgram.getSymbolTable()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

# Find any symbol/string mentioning "LeashSelector" or "Selector".
print("=== RTTI / type-descriptor strings with 'Selector' ===")
sel_addrs = []
for d in lst.getDefinedData(True):
    try: v = d.getValue()
    except: continue
    if v is None: continue
    s = str(v)
    if "Selector" in s:
        print("  0x{:08X} {!r}".format(d.getAddress().getOffset(), s))
        sel_addrs.append(d.getAddress())

# For each type-descriptor, walk references to locate the vtable (RTTI col ->
# locator -> vftable). Simpler: list xrefs to the descriptor and the funcs.
print("\n=== xrefs to Selector descriptors ===")
funcs = {}
for a in sel_addrs:
    for r in ref.getReferencesTo(a):
        fa = r.getFromAddress()
        f = af.getFunctionContaining(fa)
        nm = f.getName() if f else "?"
        print("  0x{:08X} ({}) refs 0x{:08X}".format(fa.getOffset(), nm, a.getOffset()))
        if f: funcs[f.getEntryPoint().getOffset()] = f.getName()

# Decompile the leash mesh loader FUN_004645d0 to see the collar object struct.
for entry, label in [(0x004645d0, "leash.l3d loader")]:
    f = af.getFunctionContaining(addr(entry))
    if f:
        print("=" * 78); print("=== {} {} ===".format(label, f.getName()))
        res = di.decompileFunction(f, 60, mon)
        print(res.getDecompiledFunction().getC() if res and res.decompileCompleted() else "(failed)")
