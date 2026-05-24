# Decompile the billboard-basis writer FUN_00855340 and the function that
# contains the call sites to the billboard renderer FUN_00854630, plus pull
# any string references in that subsystem, to determine whether it renders
# name labels (text banners) or particles/effects.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

# What function contains the call sites 0x0080F95B / 0x0080FEEB?
for site in (0x0080F95B, 0x0080FEEB):
    f = fm.getFunctionContaining(addr(site))
    print("call site {} is in {}".format(hex(site),
          "FUN_{:08X}".format(f.getEntryPoint().getOffset()) if f else "?"))

def strings_in(ep):
    f = fm.getFunctionAt(addr(ep))
    out = []
    if not f: return out
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            d = listing.getDataAt(r.getToAddress())
            if d and d.hasStringValue():
                out.append(d.getValue())
    return out

def dump(ep, limit=5000):
    f = fm.getFunctionAt(addr(ep))
    if not f:
        print("no fn at {:08X}".format(ep)); return
    print("=" * 78)
    print("FUN_{:08X} size={} strings={}".format(
        ep, int(f.getBody().getNumAddresses()), strings_in(ep)[:15]))
    print("=" * 78)
    res = di.decompileFunction(f, 60, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC()[:limit])

dump(0x00855340)
# Container of the billboard renderer calls:
cont = fm.getFunctionContaining(addr(0x0080F95B))
if cont:
    dump(cont.getEntryPoint().getOffset(), 6000)

print("\nDone.")
