# Find the render begin / render end boundaries.
#
# In a DirectX 7 game these are typically D3D7::BeginScene / EndScene,
# called via vtable from a top-level render driver.
#
# Strategy:
#   1. Look for "BeginScene" / "EndScene" strings and their xrefs.
#   2. Look for the engine-side render driver: functions that call both
#      a camera-builder (FUN_00819920) AND one of the heavy render
#      functions (e.g. FUN_0084DAA0).
#   3. Look for top-level functions that read g_cameraStruct/scaled and
#      have lots of internal calls to the render-side leaf functions.

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
sym  = currentProgram.getSymbolTable()
mem  = currentProgram.getMemory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

KEYWORDS = ['BeginScene', 'EndScene', 'BeginFrame', 'EndFrame',
            'RenderScene', 'DrawScene', 'Present',
            'lh3dwater', 'LH3DWater', 'WaterRender', 'WaterDraw']

print("[1] String matches for render-phase keywords...")
print("=" * 78)
data_it = listing.getDefinedData(True)
hits = []
while data_it.hasNext():
    d = data_it.next()
    if not d.hasStringValue(): continue
    s = d.getDefaultValueRepresentation() or ''
    for kw in KEYWORDS:
        if kw in s:
            hits.append((d.getAddress(), s, kw))
            break

for saddr, sval, kw in hits[:40]:
    print("  {}  [{}]  {}".format(saddr, kw, sval[:80].replace('\n',' ')))
    for r in ref.getReferencesTo(saddr):
        fr = r.getFromAddress()
        f = af.getFunctionContaining(fr)
        if f:
            print("    ref from FUN_{:08X}  ({})".format(
                f.getEntryPoint().getOffset(), f.getName()))
print("")

# Find callers of FUN_00819920 (the per-frame camera builder).
# Whichever caller ALSO calls FUN_0084DAA0 (a known render leaf) is the
# render driver.
print("[2] Functions that call BOTH the camera builder and a render leaf...")
print("=" * 78)

CAMERA_BUILDERS = [0x00819920, 0x00819f50]
RENDER_LEAVES   = [0x0084DAA0, 0x0084D2D0, 0x008792E0, 0x008794A0, 0x00852C40]

def callers_of(target):
    out = set()
    for r in ref.getReferencesTo(addr(target)):
        if r.getReferenceType().isCall():
            f = af.getFunctionContaining(r.getFromAddress())
            if f: out.add(f.getEntryPoint().getOffset())
    return out

cb_callers = set()
for t in CAMERA_BUILDERS:
    cb_callers |= callers_of(t)

rl_callers = set()
for t in RENDER_LEAVES:
    rl_callers |= callers_of(t)

print("  Camera-builder callers: {}".format(len(cb_callers)))
print("  Render-leaf callers:    {}".format(len(rl_callers)))
print("  Intersection (likely render driver):")
inter = sorted(cb_callers & rl_callers)
for ep in inter:
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    print("    FUN_{:08X}  size={}  name={}".format(ep, sz, f.getName() if f else '?'))
print("")

# Look up what calls FUN_0084DAA0 and ascend to find the render driver
print("[3] Call chain ascending from FUN_0084DAA0...")
print("=" * 78)

def callers(ep):
    f = af.getFunctionAt(addr(ep))
    out = set()
    if not f: return out
    for r in ref.getReferencesTo(addr(ep)):
        if r.getReferenceType().isCall():
            g = af.getFunctionContaining(r.getFromAddress())
            if g: out.add(g.getEntryPoint().getOffset())
    return out

current = {0x0084DAA0}
for depth in range(4):
    nxt = set()
    print("  depth {}:".format(depth))
    for ep in sorted(current):
        f = af.getFunctionAt(addr(ep))
        sz = int(f.getBody().getNumAddresses()) if f else 0
        print("    FUN_{:08X}  size={}".format(ep, sz))
        nxt |= callers(ep)
    nxt -= current
    if not nxt: break
    current = nxt

# Look for IDirect3DDevice7 vtable methods. SetTransform is slot 0x90/4=36,
# BeginScene slot 0x94/4, EndScene slot 0x98/4 for D3DDevice7 vtable.
# Simpler: scan for indirect calls following the pattern "MOV ECX, ...;
# CALL [ECX+0x94]" etc. Just dump CALL [reg+offs] where offs in {0x90, 0x94, 0x98}.
print("")
print("[4] D3D device vtable indirect-call hotspots...")
print("=" * 78)
TARGET_OFFSETS = [0x90, 0x94, 0x98, 0x9C]  # SetTransform/BeginScene/EndScene area
hits_by_off = {o: set() for o in TARGET_OFFSETS}
for blk in mem.getBlocks():
    if not blk.isInitialized(): continue
    if not blk.isExecute(): continue
    a = blk.getStart()
    end = blk.getEnd()
    while a is not None and a.compareTo(end) <= 0:
        ins = listing.getInstructionAt(a)
        if ins is not None:
            if ins.getMnemonicString() == 'CALL' and ins.getNumOperands() >= 1:
                op = ins.getDefaultOperandRepresentation(0)
                for off in TARGET_OFFSETS:
                    tag = '0x{:x}'.format(off)
                    if tag in op or '+ {}'.format(off) in op:
                        f = af.getFunctionContaining(a)
                        if f:
                            hits_by_off[off].add(f.getEntryPoint().getOffset())
            a = ins.getMaxAddress().next()
        else:
            a = a.next()

for off, fns in hits_by_off.items():
    print("  CALL [reg+0x{:x}]:  {} functions".format(off, len(fns)))
    for ep in sorted(fns)[:10]:
        f = af.getFunctionAt(addr(ep))
        sz = int(f.getBody().getNumAddresses()) if f else 0
        print("    FUN_{:08X}  size={}".format(ep, sz))

print("Done.")
