# Find render functions that (a) reference a camera matrix global and
# (b) reference the IDirect3DDevice7* global (i.e. they draw), but are
# NOT reachable from the world-render dispatcher FUN_0054DA80 that our
# hook sandwiches. Those render with a camera matrix our sandwich never
# corrects -> drift candidates for the name-label / HUD-marker bug.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

SANDWICH_ROOT = 0x0054DA80
DEVICE_GLOBAL = 0x00ECA638

CAMERA_STATE = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
    0x00EA1DC4: 'g_cameraTarget',
}

def fn_at(a):
    return fm.getFunctionContaining(a)

def callees(fn):
    """Functions called from within fn."""
    out = set()
    for a in fn.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            if r.getReferenceType().isCall():
                f = fm.getFunctionContaining(r.getToAddress())
                if f:
                    out.add(f.getEntryPoint().getOffset())
    return out

# 1. Forward-reachable set from the sandwiched dispatcher.
print("Building reachable set from FUN_{:08X}...".format(SANDWICH_ROOT))
reachable = set()
stack = [SANDWICH_ROOT]
while stack:
    ep = stack.pop()
    if ep in reachable: continue
    reachable.add(ep)
    f = fm.getFunctionAt(addr(ep))
    if not f: continue
    for c in callees(f):
        if c not in reachable:
            stack.append(c)
print("  reachable functions: {}".format(len(reachable)))

# 2. Functions referencing the device global.
def refs_to(target):
    fns = set()
    for r in ref.getReferencesTo(addr(target)):
        f = fm.getFunctionContaining(r.getFromAddress())
        if f:
            fns.add(f.getEntryPoint().getOffset())
    return fns

device_users = refs_to(DEVICE_GLOBAL)
print("  functions referencing device global: {}".format(len(device_users)))

# 3. For each device-user, which camera globals does it read?
def cam_reads(fn):
    reads = set()
    for a in fn.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t = r.getToAddress().getOffset()
                if t in CAMERA_STATE:
                    reads.add(CAMERA_STATE[t])
    return reads

print("")
print("=" * 78)
print("Device-drawing functions that read a camera global but are OUTSIDE")
print("the world-render sandwich (drift candidates):")
print("=" * 78)
rows = []
for ep in device_users:
    f = fm.getFunctionAt(addr(ep))
    if not f: continue
    reads = cam_reads(f)
    if not reads: continue
    in_sandwich = ep in reachable
    rows.append((in_sandwich, ep, f.getName(),
                 int(f.getBody().getNumAddresses()), sorted(reads)))

for in_sw, ep, nm, sz, reads in sorted(rows, key=lambda r: (r[0], r[1])):
    mark = "IN-sandwich " if in_sw else "OUT         "
    print("  [{}] FUN_{:08X} {:<28} size={:<6} reads={}".format(
        mark, ep, nm, sz, reads))

print("\nDone.")
