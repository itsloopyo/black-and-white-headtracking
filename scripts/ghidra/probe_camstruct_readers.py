# For each candidate render function that reads g_cameraStruct, print:
#   - its callers (with names)
#   - any string references reachable directly in its body
#   - a decompile so we can spot billboard/label/text projection patterns
#
# Goal: identify which one draws the world-anchored name-label background.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
data = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

CANDIDATES = [
    0x0084D2D0, 0x0084DAA0, 0x0084F2F0,
    0x00852C40, 0x00853A50, 0x00854630,
]

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def string_refs(fn):
    out = []
    for a in fn.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            d = currentProgram.getListing().getDataAt(r.getToAddress())
            if d and d.hasStringValue():
                out.append((str(r.getToAddress()), d.getValue()))
    return out

def callers(ep):
    out = []
    for r in ref.getReferencesTo(addr(ep)):
        if r.getReferenceType().isCall():
            f = fm.getFunctionContaining(r.getFromAddress())
            if f: out.append(f.getEntryPoint().getOffset())
    return sorted(set(out))

for t in CANDIDATES:
    f = fm.getFunctionAt(addr(t))
    print("=" * 78)
    print("FUN_{:08X}  size={}".format(t, int(f.getBody().getNumAddresses())))
    print("  callers: {}".format([hex(c) for c in callers(t)]))
    srs = string_refs(f)
    if srs:
        print("  strings: {}".format(srs[:20]))
    print("=" * 78)
    res = di.decompileFunction(f, 60, mon)
    if res and res.decompileCompleted():
        c = res.getDecompiledFunction().getC()
        # Trim very long bodies to keep output readable.
        print(c[:4000])
    print("")

print("Done.")
