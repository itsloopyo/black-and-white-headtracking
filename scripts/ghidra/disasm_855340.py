# Disassemble FUN_00855340 fully so we can read which matrix is fed into the
# inverter FUN_007FB290 (ecx=out, edx=in for __fastcall) to produce the
# billboard matrix at 0xEA1CC8, and what 0xEA9E90 (the transformed point) is.
# Also disassemble who computes/loads 0xEA1CC8 elsewhere.

fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
fm = currentProgram.getFunctionManager()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def dump(ep):
    f = fm.getFunctionAt(addr(ep))
    print("=" * 78)
    print("FUN_{:08X}".format(ep))
    print("=" * 78)
    ins = listing.getInstructionAt(addr(ep))
    end = f.getBody().getMaxAddress()
    while ins and ins.getAddress().compareTo(end) <= 0:
        print("  {}  {}".format(ins.getAddress(), ins))
        ins = ins.getNext()

dump(0x00855340)
print("\nDone.")
