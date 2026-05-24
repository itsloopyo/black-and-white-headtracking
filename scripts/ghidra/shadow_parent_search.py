# Walk up the caller tree of FUN_0081FFF0 (shadow renderer) and check
# each ancestor for reachability from FUN_0054DA80. Find the closest
# ancestor that is OR find one that is the right scope to sandwich.
#
# Also: find the topmost ancestors (frame-tick or render-tick entry).

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

reachable_dispatcher = set()
def build_reach(root):
    stack = [root]
    while stack:
        ep = stack.pop()
        if ep in reachable_dispatcher: continue
        reachable_dispatcher.add(ep)
        f = af.getFunctionAt(addr(ep))
        if not f: continue
        for c in f.getCalledFunctions(ConsoleTaskMonitor()):
            if c: stack.append(c.getEntryPoint().getOffset())
build_reach(0x0054DA80)

def caller_chain(tgt, max_depth=6):
    """BFS up: print every ancestor with reachability flag."""
    seen = {tgt}
    layer = [(tgt, 0)]
    while layer:
        new_layer = []
        for ep, depth in layer:
            if depth >= max_depth: continue
            for r in ref.getReferencesTo(addr(ep)):
                if not r.getReferenceType().isCall(): continue
                f = af.getFunctionContaining(r.getFromAddress())
                if not f: continue
                pep = f.getEntryPoint().getOffset()
                if pep in seen: continue
                seen.add(pep)
                sz = int(f.getBody().getNumAddresses())
                inscope = '[IN]' if pep in reachable_dispatcher else '[OUT]'
                print("  depth={}  FUN_{:08X}  size={:5d}  {}".format(
                    depth + 1, pep, sz, inscope))
                new_layer.append((pep, depth + 1))
        layer = new_layer

print("Caller BFS up from FUN_0081FFF0:")
print("=" * 78)
caller_chain(0x0081FFF0)

print("")
print("Caller BFS up from FUN_00817930:")
print("=" * 78)
caller_chain(0x00817930)

print("")
print("Caller BFS up from FUN_00815A70:")
print("=" * 78)
caller_chain(0x00815A70)
