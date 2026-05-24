# Determine calling convention of FUN_0081FFF0 by inspecting Ghidra's
# function signature AND the prologue instructions to see how arguments
# are pulled.

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

TARGETS = [0x0081FFF0, 0x00815A70, 0x00817930, 0x00879930, 0x0054DA80]

for t in TARGETS:
    f = af.getFunctionAt(addr(t))
    if not f:
        print("FUN_{:08X}: not found".format(t)); continue
    print("=" * 78)
    print("FUN_{:08X}".format(t))
    print("  name           : {}".format(f.getName()))
    print("  calling conv   : {}".format(f.getCallingConventionName()))
    print("  signature      : {}".format(f.getSignature().getPrototypeString()))
    print("  paramCount     : {}".format(f.getParameterCount()))
    for i, p in enumerate(f.getParameters()):
        print("    arg{}: name={}  type={}  storage={}".format(
            i, p.getName(), p.getDataType().getName(),
            p.getVariableStorage()))
    # First 20 instructions:
    print("  prologue:")
    a = f.getEntryPoint()
    for _ in range(20):
        ins = listing.getInstructionAt(a)
        if not ins: break
        print("    {}  {}".format(a, ins))
        a = ins.getNext().getMinAddress() if ins.getNext() else None
        if not a: break
    print("")
