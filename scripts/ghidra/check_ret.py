# Examine ret instructions of FUN_0081FFF0 to distinguish cdecl vs stdcall.
# cdecl: RET (no operand)
# stdcall: RET 0xC (or similar - pops N bytes)

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

for tgt in [0x0081FFF0, 0x0081FE10]:
    f = af.getFunctionAt(addr(tgt))
    if not f: continue
    print("=" * 78)
    print("FUN_{:08X} returns:".format(tgt))
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        mn = ins.getMnemonicString()
        if mn.startswith('RET'):
            print("  {}  {}".format(a, ins))
    # Also dump last 20 instructions
    print("  --- tail ---")
    addrs = []
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if ins: addrs.append((a, ins))
    for a, ins in addrs[-15:]:
        print("    {}  {}".format(a, ins))
