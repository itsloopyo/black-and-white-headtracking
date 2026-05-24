# Dump initial values of 0xEA9DE0 (and neighbors) and hunt for what
# writes to it. Likely candidates: a function that does memcpy or
# REP MOVSD into [0xEA9DE0].

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import struct

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
mem  = currentProgram.getMemory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

print("Initial bytes / floats at 0xEA9DE0..0xEA9E40:")
print("=" * 78)
for off in range(0, 0x60, 4):
    a = 0x00EA9DE0 + off
    try:
        v = mem.getInt(addr(a)) & 0xFFFFFFFF
        # Interpret as float
        fv = struct.unpack('<f', struct.pack('<I', v))[0]
        print("  0x{:08X}  raw=0x{:08X}  float={}".format(a, v, fv))
    except Exception as e:
        print("  0x{:08X}  (uninit)".format(a))

# Now find functions whose body contains the literal 0xEA9DE0 in any
# context, and dump the surrounding 10 instructions of each occurrence.
print("")
print("All occurrences of literal 0xEA9DE0 in code:")
print("=" * 78)
for r in ref.getReferencesTo(addr(0x00EA9DE0)):
    fr = r.getFromAddress()
    ins = listing.getInstructionAt(fr)
    if not ins: continue
    f = af.getFunctionContaining(fr)
    if not f: continue
    ep = f.getEntryPoint().getOffset()
    print("")
    print("--- in FUN_{:08X}  size={} ---".format(ep, int(f.getBody().getNumAddresses())))
    # Show 5 before, this, 5 after
    addrs = []
    a = ins
    for _ in range(5):
        p = a.getPrevious()
        if not p: break
        a = p
    for _ in range(12):
        if not a: break
        marker = '>>>' if a.getAddress().equals(fr) else '   '
        print("  {}  {}  {}".format(marker, a.getAddress(), a))
        a = a.getNext()
