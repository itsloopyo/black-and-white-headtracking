# Enumerate every caller of the two known pick utilities and classify them so we
# can find which pick paths the creature mouseover + mission-scroll mouseover
# share with the citadel collar pick. The expectation: if all three interactables
# route through FUN_00519960 (object-screen-pick) or FUN_0081B370 (screen->world
# ray), there's a single fix surface. If they DON'T, we need to find the parallel
# pick path.
#
# Outputs (printed):
#  1. All call sites of FUN_00519960 (object-screen-pick): caller function entry,
#     caller function size, call-from address (return address = +5 typically).
#  2. All call sites of FUN_0081B370 (screen-to-world ray): same shape.
#  3. For each caller function: whether it reads kCursorX (0x00E852C0) -
#     marks it as a cursor-driven pick rather than e.g. a procedural ray cast.
#  4. For each caller function: whether it's inside the world-render sandwich
#     (i.e. reachable from FUN_0054DA80 dispatcher).

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

PICK_OBJ = 0x00519960
S2W      = 0x0081B370
CURSOR_X = 0x00E852C0
CURSOR_Y = 0x00E852C4

def caller_functions_of(target):
    """Return list of (call_site_addr, containing_function) for each call to target."""
    out = []
    for r in ref.getReferencesTo(addr(target)):
        if not r.getReferenceType().isCall(): continue
        f = af.getFunctionContaining(r.getFromAddress())
        if not f: continue
        out.append((r.getFromAddress().getOffset(), f))
    return out

def function_reads(func, target_addr):
    """Does this function reference target_addr anywhere in its body?"""
    body = func.getBody()
    tgt = addr(target_addr)
    for a in body.getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            if r.getToAddress() == tgt:
                return True
    return False

# Build a coarse "reachable from dispatcher" set by walking call graph
# from FUN_0054DA80 (world render). Cap at depth 8.
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
print("[info] {} functions reachable from FUN_{:08X}".format(len(reachable), WORLD_DISP))

def dump(target_addr, label):
    print("")
    print("=" * 78)
    print("### Callers of FUN_{:08X} ({}) ###".format(target_addr, label))
    print("=" * 78)
    callers = caller_functions_of(target_addr)
    # Group by containing function so we don't print one func per call site.
    by_func = {}
    for site, f in callers:
        ep = f.getEntryPoint().getOffset()
        by_func.setdefault(ep, (f, []))[1].append(site)
    for ep in sorted(by_func.keys()):
        f, sites = by_func[ep]
        sz = int(f.getBody().getNumAddresses())
        reads_cursor = function_reads(f, CURSOR_X) or function_reads(f, CURSOR_Y)
        in_world = ep in reachable
        flags = []
        if reads_cursor: flags.append("CURSOR")
        if in_world:     flags.append("IN-WORLD-RENDER")
        flag_str = " ".join(flags) if flags else "-"
        print("  FUN_{:08X} size={:5d} [{}] sites={}".format(
            ep, sz, flag_str, ",".join("0x{:08X}".format(s) for s in sites)))

dump(PICK_OBJ, "object screen pick")
dump(S2W,      "screen to world ray")

# Also list functions that BOTH read kCursorX AND call one of the projectors.
# These are the strongest "this is a mouseover pick" candidates.
print("")
print("=" * 78)
print("### Strong candidates: read cursor AND call object-pick or S2W ###")
print("=" * 78)
projector_addrs = [PICK_OBJ, S2W]
strong = set()
for r in ref.getReferencesTo(addr(CURSOR_X)):
    f = af.getFunctionContaining(r.getFromAddress())
    if not f: continue
    ep = f.getEntryPoint().getOffset()
    # Does this function also call PICK_OBJ or S2W?
    for callee_addr in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(callee_addr)
        if not ins: continue
        for cr in ins.getReferencesFrom():
            if not cr.getReferenceType().isCall(): continue
            if cr.getToAddress().getOffset() in projector_addrs:
                strong.add(ep)
                break
for ep in sorted(strong):
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    sz = int(f.getBody().getNumAddresses())
    in_world = ep in reachable
    print("  FUN_{:08X} size={:5d} {}".format(
        ep, sz, "(in world-render)" if in_world else ""))

print("\n[done]")
