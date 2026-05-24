# Locate the god-hand cursor render path in B&W.
#
# Likely strings: "Hand", "Finger", "Cursor", references to hand mesh.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Scan defined strings for any of these tokens.
TOKENS = ['Hand', 'hand', 'Finger', 'Cursor', 'cursor', 'godhand', 'GodHand', 'finger']

found = []
it = listing.getDefinedData(True)
count = 0
while it.hasNext() and count < 250000:
    d = it.next()
    count += 1
    if not d.hasStringValue(): continue
    try:
        s = d.getDefaultValueRepresentation()
    except: continue
    s_clean = s.strip('"')
    for tok in TOKENS:
        if tok in s_clean:
            # filter ones that are clearly not relevant (e.g. ones in long sentences)
            if len(s_clean) > 80: continue
            found.append((d.getAddress().getOffset(), s_clean))
            break

print("Strings matching hand/finger/cursor tokens:")
print("=" * 78)
for a, s in found[:80]:
    print("  0x{:08X}  {}".format(a, s[:80]))

# Find functions referencing those strings.
print("")
print("Functions referencing those strings:")
print("=" * 78)
fn_refs = {}
for str_addr, s in found:
    for r in ref.getReferencesTo(addr(str_addr)):
        f = af.getFunctionContaining(r.getFromAddress())
        if f:
            ep = f.getEntryPoint().getOffset()
            fn_refs.setdefault(ep, []).append(s[:40])

# Build reachability from FUN_0054DA80.
reach = set()
stack = [0x0054DA80]
while stack:
    ep = stack.pop()
    if ep in reach: continue
    reach.add(ep)
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    for c in f.getCalledFunctions(ConsoleTaskMonitor()):
        if c: stack.append(c.getEntryPoint().getOffset())

for ep in sorted(fn_refs):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    inscope = '[IN sandwich]' if ep in reach else '[OUT]'
    print("  FUN_{:08X} size={:5d} {} strings={}".format(
        ep, sz, inscope, fn_refs[ep][:3]))
