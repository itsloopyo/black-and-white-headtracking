from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

for ep in [0x0083A1D0, 0x00839F10]:
    f = af.getFunctionAt(addr(ep))
    body = f.getBody()
    it = listing.getInstructions(body, True)
    print("=" * 78)
    print("FUN_{:08X}".format(ep))
    print("=" * 78)
    while it.hasNext():
        ins = it.next()
        print("  {}  {}".format(ins.getAddress(), ins))
    print("")
