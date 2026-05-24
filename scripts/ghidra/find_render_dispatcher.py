# Find callers of LH3DWater::Render (0x00879930). The outer dispatcher
# is our candidate for a render-frame pre/post sandwich.
#
# Also dump callers of FUN_00819920 (per-frame camera updater) so we can
# see how the render dispatcher relates to camera update timing.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

TARGETS = [
    (0x00879930, 'LH3DWater::Render'),
    (0x00819920, 'FUN_00819920 camera updater'),
    (0x00819F50, 'FUN_00819F50 cutscene camera'),
]

CAMERA_STATE = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
}

def fn_at(a):
    return af.getFunctionContaining(a)

def reads_in_fn(fn, watch):
    """Return set of WATCH names this fn references."""
    reads = set()
    for a in fn.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t = r.getToAddress().getOffset()
                if t in watch:
                    reads.add(watch[t])
    return reads

for tgt, label in TARGETS:
    print("=" * 78)
    print("Callers of 0x{:08X} ({}):".format(tgt, label))
    print("=" * 78)
    callers = {}
    for r in ref.getReferencesTo(addr(tgt)):
        if r.getReferenceType().isCall():
            fr = r.getFromAddress()
            f = fn_at(fr)
            if f:
                ep = f.getEntryPoint().getOffset()
                callers.setdefault(ep, []).append(fr)
    for ep, sites in sorted(callers.items()):
        f = af.getFunctionAt(addr(ep))
        sz = int(f.getBody().getNumAddresses()) if f else 0
        nm = f.getName() if f else '?'
        r = reads_in_fn(f, CAMERA_STATE) if f else set()
        rmark = ' [reads: {}]'.format(sorted(r)) if r else ''
        print("  FUN_{:08X} {}  size={}  {} call sites{}".format(ep, nm, sz, len(sites), rmark))
    print("")

# Now: take the unique callers of LH3DWater::Render and look at THEIR
# callers - one level up. This usually surfaces the render-frame entry.
print("=" * 78)
print("Two-level call tree up from LH3DWater::Render:")
print("=" * 78)
seen = set()
def walk_callers(target_addr, depth, max_depth=3):
    if depth > max_depth: return
    if target_addr in seen: return
    seen.add(target_addr)
    for r in ref.getReferencesTo(addr(target_addr)):
        if not r.getReferenceType().isCall(): continue
        f = fn_at(r.getFromAddress())
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        sz = int(f.getBody().getNumAddresses())
        reads = reads_in_fn(f, CAMERA_STATE)
        rmark = ' [reads: {}]'.format(sorted(reads)) if reads else ''
        print("  {}-> FUN_{:08X}  size={}{}".format('  ' * depth, ep, sz, rmark))
        walk_callers(ep, depth + 1, max_depth)

walk_callers(0x00879930, 0)
print("\nDone.")
