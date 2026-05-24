# Dump the last ~120 bytes of FUN_00819920 and FUN_00819F50 so we can
# see by eye which global holds the IDirect3DDevice7* and what the
# SetTransform call looks like.

af = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
fm = currentProgram.getFunctionManager()

def addr(x):
    return af.getAddress(hex(x).rstrip('L'))

def dump_tail(fn_start, n_bytes=200):
    fn = fm.getFunctionAt(addr(fn_start))
    if fn is None:
        print("no fn at {:08X}".format(fn_start)); return
    body = fn.getBody()
    end_addr = body.getMaxAddress()
    start_addr = end_addr.subtract(n_bytes)
    if start_addr.compareTo(body.getMinAddress()) < 0:
        start_addr = body.getMinAddress()
    print("=" * 78)
    print("FUN_{:08X} tail  {} .. {}".format(fn_start, start_addr, end_addr))
    print("=" * 78)
    instr = listing.getInstructionAt(start_addr)
    while instr is not None and instr.getAddress().compareTo(end_addr) <= 0:
        site = instr.getAddress()
        bytestr = " ".join("{:02X}".format(b & 0xFF) for b in instr.getBytes())
        print("  {}  {:<24}  {}".format(site, bytestr, instr))
        instr = instr.getNext()
    print("")

dump_tail(0x00819920, 200)
dump_tail(0x00819F50, 200)
