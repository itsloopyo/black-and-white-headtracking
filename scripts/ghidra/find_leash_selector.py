# Find the GLeashSelector class methods that do the cursor->collar pick.
# Strategy: find RTTI/vtable for any "LeashSelector"/"Selector" class, list
# the virtual methods, and decompile FUN_00476070 ("Leash clicked") plus any
# selector method that calls FUN_0081b370 or reads the mouse.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
lst  = currentProgram.getListing()
mem  = currentProgram.getMemory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def decompile(entry, label):
    f = af.getFunctionContaining(addr(entry))
    if not f:
        print("{} 0x{:08X}: no function".format(label, entry)); return
    print("=" * 78)
    print("=== {} {} entry=0x{:08X} size={}".format(
        label, f.getName(), f.getEntryPoint().getOffset(),
        int(f.getBody().getNumAddresses())))
    print("=" * 78)
    res = di.decompileFunction(f, 90, mon)
    print(res.getDecompiledFunction().getC() if res and res.decompileCompleted()
          else "(decompile failed)")

# 1) "Leash clicked" handler.
decompile(0x00476070, "Leash-debug/update")

# 2) Find functions whose name or nearby strings suggest selector; instead,
#    list all functions that reference the leash.l3d mesh loader region and
#    call FUN_0081b370. Quicker: scan every function for a call to 0081b370
#    AND a read of the mouse/cursor, that also lives near the leash code.
print("\n=== functions calling FUN_0081b370 ===")
b370 = addr(0x0081B370)
callers = set()
for r in ref.getReferencesTo(b370):
    f = af.getFunctionContaining(r.getFromAddress())
    if f:
        callers.add((f.getEntryPoint().getOffset(), f.getName()))
for entry, name in sorted(callers):
    print("  0x{:08X} {}".format(entry, name))
