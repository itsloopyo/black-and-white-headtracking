fact = currentProgram.getAddressFactory(); lst = currentProgram.getListing(); af = currentProgram.getFunctionManager()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))
def dis(start, n, label):
    print("--- {} 0x{:08X} ---".format(label, start)); a=addr(start)
    for _ in range(n):
        ins=lst.getInstructionAt(a)
        if ins is None: print("  (none)"); break
        print("  0x{:08X}: {}".format(a.getOffset(), ins.toString())); a=a.add(ins.getLength())
for e in (0x00466730, 0x00519960):
    f=af.getFunctionContaining(addr(e)); mx=f.getBody().getMaxAddress().getOffset()
    dis(e,6,"prologue"); dis(mx-5,5,"tail")
