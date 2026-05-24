# Bare RET = __cdecl; RET imm16 = __stdcall (callee pops). Also count
# args from prologue stack reads to confirm width.

from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def scan(ep, label):
    f = af.getFunctionAt(addr(ep))
    if not f:
        print("{}: no function".format(label))
        return
    body = f.getBody()
    it = listing.getInstructions(body, True)
    rets = []
    while it.hasNext():
        ins = it.next()
        s = str(ins)
        if s.startswith("RET"):
            rets.append((str(ins.getAddress()), s))
    print("{}  FUN_{:08X}  size={}".format(label, ep, f.getBody().getNumAddresses()))
    if not rets:
        print("  (no RET found - tail-call only?)")
    for a, s in rets[:6]:
        print("  {}  {}".format(a, s))
    print("")

for ep, lbl in [(0x0083A1D0, "A1D0"), (0x00839F10, "9F10"), (0x00839980, "9980 (thiscall?)"), (0x00839BC0, "9BC0")]:
    scan(ep, lbl)
