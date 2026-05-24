# Investigate the cursor-snapshot path: FUN_00542A90 writes DAT_00ea1b10/14
# from kCursorX/Y. If pick functions read from THAT snapshot rather than
# kCursorX/Y directly, and the snapshot is taken at a different camera state
# than the pick uses, we have a timing-mismatch fix surface.
#
# Outputs:
#  1. All readers of DAT_00ea1b10 (the X snapshot) and DAT_00ea1b14 (Y snapshot)
#     - these are the functions that USE the snapshot.
#  2. All callers of FUN_00542A90 - when is the snapshot taken?
#  3. Whether each reader/caller is inside the world-render sandwich.
#  4. Quick decompile of FUN_00542A90's callers + any small readers, to see the
#     usage pattern.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram); mon = ConsoleTaskMonitor()

# Build reachable-from-world-render set.
WORLD_DISP = 0x0054DA80
reachable = set()
def expand(ep, depth):
    if depth > 8: return
    if ep in reachable: return
    reachable.add(ep)
    f = af.getFunctionAt(addr(ep))
    if not f: return
    for callee_addr in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(callee_addr)
        if not ins: continue
        for r in ins.getReferencesFrom():
            if not r.getReferenceType().isCall(): continue
            cf = af.getFunctionAt(r.getToAddress())
            if cf:
                expand(cf.getEntryPoint().getOffset(), depth + 1)
expand(WORLD_DISP, 0)

def dump_refs(target_addr, label):
    print("")
    print("=" * 78)
    print("### References to 0x{:08X} ({}) ###".format(target_addr, label))
    print("=" * 78)
    by_func = {}
    for r in ref.getReferencesTo(addr(target_addr)):
        f = af.getFunctionContaining(r.getFromAddress())
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        rt = str(r.getReferenceType())
        by_func.setdefault(ep, (f, [])) [1].append((r.getFromAddress().getOffset(), rt))
    for ep in sorted(by_func.keys()):
        f, sites = by_func[ep]
        sz = int(f.getBody().getNumAddresses())
        in_world = ep in reachable
        ref_types = set(rt for _, rt in sites)
        kind = "/".join(sorted(ref_types))
        print("  FUN_{:08X} size={:5d} {}  refs={}  {}".format(
            ep, sz,
            "(in world-render)" if in_world else "",
            kind,
            ",".join("0x{:08X}".format(s) for s, _ in sites)))

# Cursor snapshot pair
dump_refs(0x00EA1B10, "cursorSnapX")
dump_refs(0x00EA1B14, "cursorSnapY")

# Callers of the function that writes the snapshot
print("")
print("=" * 78)
print("### Callers of FUN_00542A90 (writes the cursor snapshot) ###")
print("=" * 78)
for r in ref.getReferencesTo(addr(0x00542A90)):
    if not r.getReferenceType().isCall(): continue
    f = af.getFunctionContaining(r.getFromAddress())
    if not f: continue
    ep = f.getEntryPoint().getOffset()
    in_world = ep in reachable
    print("  call@0x{:08X}  in FUN_{:08X} ({}, size={})".format(
        r.getFromAddress().getOffset(), ep,
        "in world-render" if in_world else "outside world-render",
        int(f.getBody().getNumAddresses())))

# Decompile the small reader candidates so we can see what they do with the snapshot.
def show(ep, label):
    print("")
    print("#" * 78)
    print("### {}  FUN_{:08X} ###".format(label, ep))
    print("#" * 78)
    f = af.getFunctionAt(addr(ep))
    if not f:
        print("(no function)"); return
    sz = int(f.getBody().getNumAddresses())
    print("size={} bytes".format(sz))
    if sz > 3000:
        print("(too large; skipping decompile)")
        return
    res = di.decompileFunction(f, 120, mon)
    if res and res.decompileCompleted():
        c = res.getDecompiledFunction().getC()
        print(c[:10000])
    else:
        print("(decompile failed)")

# Find small reader functions of 0x00EA1B10 (not write-only)
small_readers = set()
for tgt in (0x00EA1B10, 0x00EA1B14):
    for r in ref.getReferencesTo(addr(tgt)):
        if r.getReferenceType().isWrite(): continue
        f = af.getFunctionContaining(r.getFromAddress())
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        sz = int(f.getBody().getNumAddresses())
        if sz <= 2500:
            small_readers.add(ep)

print("\n=== Small readers of the cursor snapshot ({} funcs) ===".format(len(small_readers)))
for ep in sorted(small_readers)[:8]:  # cap at 8 to keep output manageable
    show(ep, "snapshot reader")

print("\n[done]")
