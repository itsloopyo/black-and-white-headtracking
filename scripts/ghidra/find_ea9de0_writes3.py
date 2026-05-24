# Direct reference-manager query for all refs to 0xEA9DE0 and any address
# in the matrix range [0xEA9DE0, 0xEA9DE0 + 0x30). Print each ref with the
# containing instruction so we can classify writes vs reads vs DATA.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
ref = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

print("ALL refs into 0xEA9DE0..0xEA9DE0+0x30:")
print("=" * 78)
for off in range(0, 0x30, 4):
    t = 0x00EA9DE0 + off
    refs = list(ref.getReferencesTo(addr(t)))
    if not refs: continue
    print("\n[offset +0x{:02X} at 0x{:08X}]".format(off, t))
    for r in refs:
        fr = r.getFromAddress()
        ins = listing.getInstructionAt(fr)
        f = af.getFunctionContaining(fr)
        ep = f.getEntryPoint().getOffset() if f else 0
        rt = r.getReferenceType().getName()
        print("  {} in FUN_{:08X}  type={}  ins={}".format(
            fr, ep, rt, ins))
