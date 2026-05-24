# Identify the god-hand cursor pick: dump each FUN_0081b370 call site inside the
# in-sandwich callers (return addresses we could exempt), and decompile the
# short in-sandwich callers so we can spot which reads the cursor coords and
# computes the hand world point. Also list readers of kCursorX (0x00E852C0) to
# find the arrow-draw path.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

# in-sandwich b370 callers worth inspecting (exclude water 879930 + dispatcher 5e42e0)
in_sandwich = [0x005267D0, 0x005BC0A0, 0x005BD2A0, 0x005C0700, 0x0074CBD0]

print("### b370 call sites (return addresses) per caller ###")
for r in ref.getReferencesTo(addr(0x0081b370)):
    if not r.getReferenceType().isCall(): continue
    fa = r.getFromAddress()
    f = af.getFunctionContaining(fa)
    ep = f.getEntryPoint().getOffset() if f else 0
    if ep in in_sandwich:
        print("  call@0x{:08X} ret~0x{:08X} in FUN_{:08X}".format(
            fa.getOffset(), fa.getOffset() + 5, ep))

print("")
print("### readers of kCursorX 0x00E852C0 (arrow draw + pick coord source) ###")
for r in ref.getReferencesTo(addr(0x00E852C0)):
    fa = r.getFromAddress()
    f = af.getFunctionContaining(fa)
    print("  0x{:08X} {} in {}".format(
        fa.getOffset(), r.getReferenceType(),
        f.getName() if f else "?"))

print("")
for ep in in_sandwich:
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    print("=" * 78)
    print("=== FUN_{:08X} size={} ===".format(ep, int(f.getBody().getNumAddresses())))
    res = di.decompileFunction(f, 60, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC())
    else:
        print("(decompile failed)")
