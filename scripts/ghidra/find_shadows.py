# Locate shadow rendering code in B&W.
#
# Strategy:
#  [1] Grep for "shadow" / "Shadow" strings, find functions that
#      reference them.
#  [2] Enumerate functions that read camera state (g_cameraStruct or
#      g_cameraPivot) and are NOT already known (camera updaters,
#      water, render dispatcher) - shadows likely fall in that set.
#  [3] Check whether each candidate is reachable from FUN_0054DA80
#      (frame dispatcher) -- if yes, it's already in our sandwich;
#      if no, it runs outside and may be the leak.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
mem  = currentProgram.getMemory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

KNOWN = {
    0x00819920: 'cameraUpdater',
    0x00819F50: 'cutsceneCamera',
    0x00879930: 'LH3DWater::Render',
    0x0054DA80: 'frameRenderDispatcher',
    0x005E42E0: 'worldRender',
    0x0081b370: 'screenToWorld',
}

CAMERA_STATE = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA1DB8: 'g_cameraPivot',
    0x00EA1DC4: 'g_cameraTo',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
}

# ----- [1] String hunt -----
print("=" * 78)
print("[1] Strings containing 'shadow' / 'Shadow':")
print("=" * 78)
shadow_strs = []
it = listing.getDefinedData(True)
count = 0
while it.hasNext() and count < 200000:
    d = it.next()
    count += 1
    if d.hasStringValue():
        try:
            s = d.getDefaultValueRepresentation()
        except: continue
        if 'shadow' in s.lower() or 'Shadow' in s:
            shadow_strs.append((d.getAddress().getOffset(), s.strip()))

print("Found {} string(s)".format(len(shadow_strs)))
for a, s in shadow_strs[:60]:
    print("  0x{:08X}  {}".format(a, s[:80]))

# Find functions referencing those strings.
print("")
print("[2] Functions referencing shadow strings:")
print("-" * 78)
fn_refs = {}
for str_addr, s in shadow_strs:
    for r in ref.getReferencesTo(addr(str_addr)):
        f = af.getFunctionContaining(r.getFromAddress())
        if f:
            ep = f.getEntryPoint().getOffset()
            fn_refs.setdefault(ep, []).append(s[:50])

for ep in sorted(fn_refs):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    print("  FUN_{:08X}  size={}  strings={}".format(ep, sz, fn_refs[ep][:3]))

# ----- [3] Camera-reading functions not in KNOWN -----
print("")
print("[3] Functions reading g_cameraStruct, excluding known:")
print("-" * 78)
camera_readers = set()
for r in ref.getReferencesTo(addr(0x00EA1D28)):
    f = af.getFunctionContaining(r.getFromAddress())
    if f:
        ep = f.getEntryPoint().getOffset()
        if ep not in KNOWN:
            camera_readers.add(ep)

# Group by size bucket to spot promising candidates.
sized = []
for ep in camera_readers:
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    sz = int(f.getBody().getNumAddresses())
    sized.append((ep, sz, f.getName()))
sized.sort(key=lambda x: -x[1])

print("Total: {} non-known g_cameraStruct readers. Top 40 by size:".format(len(sized)))
for ep, sz, nm in sized[:40]:
    print("  FUN_{:08X}  size={:5d}  {}".format(ep, sz, nm))

# ----- [4] Reachability from FUN_0054DA80 -----
print("")
print("[4] Are these camera-readers reached from FUN_0054DA80 (in sandwich)?")
print("-" * 78)
# Build call graph DFS from frame dispatcher.
reached = set()
stack = [0x0054DA80]
while stack:
    ep = stack.pop()
    if ep in reached: continue
    reached.add(ep)
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    for callee in f.getCalledFunctions(ConsoleTaskMonitor()):
        if callee:
            stack.append(callee.getEntryPoint().getOffset())

print("Total reachable from FUN_0054DA80: {}".format(len(reached)))
print("")
print("Camera-readers OUTSIDE the sandwich (these are the leaks):")
outside = [(ep, sz, nm) for (ep, sz, nm) in sized if ep not in reached]
for ep, sz, nm in outside[:30]:
    print("  FUN_{:08X}  size={:5d}  {}".format(ep, sz, nm))

print("")
print("Done.")
