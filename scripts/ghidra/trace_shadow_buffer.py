# DAT_00eb998c = the shadow material handle returned by FUN_0082fd30(6, ...).
# DAT_00eb9990 = shadow texture pointer.
# DAT_00eaa1b0, DAT_00eaa210, DAT_00eaa28c, DAT_00eaa1a0..b0 = shadow
# geometry / projection vectors.
#
# Find functions that READ any of these globals - one of them is the
# shadow renderer.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

WATCH = {
    0x00eb998c: 'g_shadowMaterial',
    0x00eb9990: 'g_shadowTexture',
    0x00eaa1b0: 'g_shadowIndices',
    0x00eaa210: 'g_shadowUVs',
    0x00eaa28c: 'g_shadowFlags',
    0x00eaa1a0: 'g_shadowProjVec',
    0x00C07168: 'sCastHumanShadow',
}

reachable_from_dispatcher = set()
def build_reach():
    stack = [0x0054DA80]
    while stack:
        ep = stack.pop()
        if ep in reachable_from_dispatcher: continue
        reachable_from_dispatcher.add(ep)
        f = af.getFunctionAt(addr(ep))
        if not f: continue
        for c in f.getCalledFunctions(ConsoleTaskMonitor()):
            if c: stack.append(c.getEntryPoint().getOffset())
build_reach()
print("Reachable from FUN_0054DA80: {}".format(len(reachable_from_dispatcher)))
print("")

# Skip the loader itself (FUN_0081FAA0) - it's setup, not render.
SKIP = {0x0081FAA0, 0x008237B0}

for target_addr, label in WATCH.items():
    print("=" * 78)
    print("Refs to {} (0x{:08X}):".format(label, target_addr))
    print("-" * 78)
    fn_refs = {}
    for r in ref.getReferencesTo(addr(target_addr)):
        fr = r.getFromAddress()
        f = af.getFunctionContaining(fr)
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        if ep in SKIP: continue
        fn_refs.setdefault(ep, 0)
        fn_refs[ep] += 1
    for ep in sorted(fn_refs):
        f = af.getFunctionAt(addr(ep))
        sz = int(f.getBody().getNumAddresses()) if f else 0
        inscope = '[IN sandwich]' if ep in reachable_from_dispatcher else '[OUTSIDE sandwich]'
        print("  FUN_{:08X}  size={:5d}  refs={}  {}".format(
            ep, sz, fn_refs[ep], inscope))

print("")
print("Done.")
