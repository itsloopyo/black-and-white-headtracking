# Decompile the heads of the six post-world passes plus FUN_0066BCD0
# ("creature name") to identify which is the HUD / name-label overlay layer.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

fm   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

TARGETS = [0x0054EC80, 0x00564160, 0x005EA980, 0x0053C480, 0x0054EB40, 0x00643420, 0x0066BCD0]

def strings_in(ep):
    f = fm.getFunctionAt(addr(ep)); out=[]
    if not f: return out
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            d = listing.getDataAt(r.getToAddress())
            if d and d.hasStringValue(): out.append(d.getValue())
    return out

for t in TARGETS:
    f = fm.getFunctionAt(addr(t))
    if not f:
        print("no fn at {:08X}".format(t)); continue
    print("=" * 78)
    print("FUN_{:08X} size={}  strings={}".format(
        t, int(f.getBody().getNumAddresses()), strings_in(t)[:10]))
    print("=" * 78)
    res = di.decompileFunction(f, 45, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC()[:1800])
    print("")

print("Done.")
