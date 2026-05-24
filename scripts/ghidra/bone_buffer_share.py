# Does the bone-update path (FUN_00817930) share its output buffer with the
# shadow projector (FUN_0081FFF0)?
#
# Strategy:
#  1. Decompile FUN_00817930 and extract every global address it writes
#     (or reads as a pointer target). PTR_DAT_00c37d9c is the suspected
#     bone-table pointer - we want to see what backing memory it resolves
#     to, and find sibling globals written nearby.
#  2. Do the same for FUN_0081FFF0 (the shadow renderer) - what does it
#     READ as bone-source data?
#  3. Intersect: addresses written by 817930 AND read by 81FFF0 are shared
#     buffers. Anything 817930 writes that 81FFF0 does NOT read is mesh-
#     only state we can safely poison with clean g_scaledMatrix.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import re

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
ref  = currentProgram.getReferenceManager()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def fn_at(ep):
    return af.getFunctionAt(addr(ep))

decomp = DecompInterface()
decomp.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def decompile_text(ep):
    f = fn_at(ep)
    if not f: return None
    res = decomp.decompileFunction(f, 120, mon)
    if not res or not res.getDecompiledFunction(): return None
    return res.getDecompiledFunction().getC()

# ---- per-function global accesses ----
# Walk every instruction in the function body and classify references as
# READ vs WRITE based on the access type Ghidra assigned. This is more
# reliable than parsing decompiler output.

def global_accesses(ep):
    """Returns (reads_set, writes_set) of global addresses touched."""
    f = fn_at(ep)
    if not f: return set(), set()
    body = f.getBody()
    reads = {}
    writes = {}
    it = listing.getInstructions(body, True)
    while it.hasNext():
        ins = it.next()
        for r in ins.getReferencesFrom():
            tgt = r.getToAddress()
            if tgt is None: continue
            off = tgt.getOffset()
            # Only interested in data segment (rough heuristic: > 0x00c00000)
            if off < 0x00c00000 or off > 0x01000000: continue
            rt = r.getReferenceType()
            ins_addr = ins.getAddress().getOffset()
            if rt.isWrite():
                writes.setdefault(off, []).append((ins_addr, str(ins)))
            elif rt.isRead():
                reads.setdefault(off, []).append((ins_addr, str(ins)))
            else:
                # Ghidra often classifies as DATA without W/R - count as read
                reads.setdefault(off, []).append((ins_addr, str(ins)))
    return reads, writes

print("=" * 78)
print("FUN_00817930 (bone updater, suspected)")
print("=" * 78)
r1, w1 = global_accesses(0x00817930)
print("\nWrites ({}):".format(len(w1)))
for off in sorted(w1):
    print("  0x{:08X}  ({} writes)".format(off, len(w1[off])))
    for ia, ins in w1[off][:2]:
        print("    @0x{:08X}  {}".format(ia, ins))
print("\nReads ({}):".format(len(r1)))
for off in sorted(r1):
    print("  0x{:08X}  ({} reads)".format(off, len(r1[off])))

print("")
print("=" * 78)
print("FUN_0081FFF0 (shadow projector, __cdecl)")
print("=" * 78)
r2, w2 = global_accesses(0x0081FFF0)
print("\nWrites ({}):".format(len(w2)))
for off in sorted(w2):
    print("  0x{:08X}  ({} writes)".format(off, len(w2[off])))
print("\nReads ({}):".format(len(r2)))
for off in sorted(r2):
    print("  0x{:08X}  ({} reads)".format(off, len(r2[off])))

print("")
print("=" * 78)
print("INTERSECTION: addresses 817930 WRITES that 81FFF0 READS")
print("=" * 78)
shared = sorted(set(w1.keys()) & set(r2.keys()))
if shared:
    for off in shared:
        print("  0x{:08X}  ({} writes / {} reads)  *** SHARED BUFFER ***".format(
            off, len(w1[off]), len(r2[off])))
else:
    print("  (none - bone update and shadow projector do not share globals directly)")

# Also follow PTR_DAT_00c37d9c - if 817930 writes to *(0xc37d9c) and 81FFF0
# reads from *(0xc37d9c), they share an indirected buffer even if their
# global writes/reads don't overlap.
print("")
print("=" * 78)
print("PTR_DAT_00c37d9c access pattern")
print("=" * 78)
PTR = 0x00c37d9c
print("\nIn FUN_00817930:")
if PTR in w1:
    print("  WRITES the pointer itself: {} times".format(len(w1[PTR])))
if PTR in r1:
    print("  READS the pointer: {} times".format(len(r1[PTR])))
print("\nIn FUN_0081FFF0:")
if PTR in w2:
    print("  WRITES the pointer itself: {} times".format(len(w2[PTR])))
if PTR in r2:
    print("  READS the pointer: {} times".format(len(r2[PTR])))
if PTR not in w1 and PTR not in r1 and PTR not in w2 and PTR not in r2:
    print("  (not touched by either function directly)")

# What about g_scaledMatrix (0xEA9E40)? Both reading it would prove they
# both consume the head-tracked matrix.
print("")
print("=" * 78)
print("g_scaledMatrix (0xEA9E40) access pattern")
print("=" * 78)
for tgt in [0xEA9E40, 0xEA9DE0, 0xEAA1A0, 0xEAA2D0]:
    in1r = tgt in r1
    in1w = tgt in w1
    in2r = tgt in r2
    in2w = tgt in w2
    if in1r or in1w or in2r or in2w:
        print("  0x{:08X}: 817930[r={},w={}]  81FFF0[r={},w={}]".format(
            tgt, in1r, in1w, in2r, in2w))

# Finally: who else writes to addresses in 81FFF0's read set? That tells us
# all the producers of the shadow's input data.
print("")
print("=" * 78)
print("Producers of shadow-input data (functions writing what 81FFF0 reads)")
print("=" * 78)
shadow_inputs = set(r2.keys())
producer_map = {}  # ep -> set of (addr it writes that 81FFF0 reads)
for tgt in shadow_inputs:
    for r in ref.getReferencesTo(addr(tgt)):
        if not r.getReferenceType().isWrite(): continue
        fr = r.getFromAddress()
        f = af.getFunctionContaining(fr)
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        if ep == 0x0081FFF0: continue
        producer_map.setdefault(ep, set()).add(tgt)
for ep in sorted(producer_map, key=lambda e: -len(producer_map[e]))[:20]:
    addrs = sorted(producer_map[ep])
    print("  FUN_{:08X}  writes {} input(s):".format(ep, len(addrs)))
    for a in addrs[:6]:
        print("    -> 0x{:08X}".format(a))

print("\nDone.")
