# Quick calling-convention check for the three bone-update functions and
# their parent. For __thiscall on MSVC x86: ECX holds `this` on entry, and
# you'll typically see "MOV reg, ECX" near the prologue. For __cdecl the
# args are all stack-loaded ([ESP+N]) and ECX is not consumed as input.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def first_n_insns(ep, n=16):
    a = addr(ep)
    out = []
    cur = listing.getInstructionAt(a)
    while cur and len(out) < n:
        out.append("  {}  {}".format(cur.getAddress(), cur))
        cur = cur.getNext()
    return out

for ep in [0x00817930, 0x0083a1d0, 0x00839f10, 0x00839980, 0x00839bc0]:
    print("FUN_{:08X}:".format(ep))
    for ln in first_n_insns(ep, 14):
        print(ln)
    # Look for a "MOV reg, ECX" anywhere in the first 20 instructions
    a = addr(ep)
    cur = listing.getInstructionAt(a)
    found_ecx_consumer = None
    for _ in range(20):
        if cur is None: break
        s = str(cur)
        if "MOV " in s and ",ECX" in s.replace(" ", ""):
            found_ecx_consumer = "{}  {}".format(cur.getAddress(), s)
            break
        cur = cur.getNext()
    print("  ECX consumed in prologue? {}".format(found_ecx_consumer or "no"))
    print("")
