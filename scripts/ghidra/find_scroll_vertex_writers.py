# Scroll mouseover-pick uses FUN_008587B0/8589B0 which do an in-triangle test of
# the cursor snapshot against polygon vertices at DAT_00e437e0. With my cursor
# shift the snapshot has the correct rotated-screen value, so if those vertices
# are at rotated-screen positions the pick should already succeed - and it
# doesn't, per user testing. Hypothesis: vertices are written through the CLEAN
# projection (or some intermediate state) so they don't align with the visible
# drawing.
#
# Find all writers of DAT_00e437e0 + surrounding range. Decompile the smallest
# ones to see what matrix they use.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram); mon = ConsoleTaskMonitor()

VERT_BUF = 0x00E437E0

print("=== All references to DAT_00e437e0 (and ±0x40 around it) ===")
seen_funcs = set()
for offset in range(-4, 64, 4):  # vertex stride is 10*4=40 bytes
    target = VERT_BUF + offset
    for r in ref.getReferencesTo(addr(target)):
        f = af.getFunctionContaining(r.getFromAddress())
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        if ep in seen_funcs: continue
        seen_funcs.add(ep)
        rt = str(r.getReferenceType())
        sz = int(f.getBody().getNumAddresses())
        print("  FUN_{:08X} size={:5d} {} ref-type={}".format(
            ep, sz, f.getName(), rt))

# Decompile likely vertex-writer candidates: smaller functions that reference
# the buffer.
NAMES = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9DE0: 'g_invScaledMatrix',
    0x00EA1DB8: 'g_cameraPivot',
    0x00E852C0: 'kCursorX',
    0x00E852C4: 'kCursorY',
    0x00EA1AC8: 'SNAP_X',
    0x00EA1ACC: 'SNAP_Y',
    0x00E437E0: 'vertBuf_x',
    0x00E437E4: 'vertBuf_y',
}

def show(ep, label):
    f = af.getFunctionAt(addr(ep))
    if not f: return
    sz = int(f.getBody().getNumAddresses())
    print("")
    print("=" * 78)
    print("=== {}  FUN_{:08X}  size={} ===".format(label, ep, sz))
    print("=" * 78)
    body = f.getBody()
    print("global refs:")
    for a in body.getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            t = r.getToAddress().getOffset()
            if t in NAMES:
                print("  0x{:08X}  {:18s}  {}".format(a.getOffset(), NAMES[t], str(ins)))
    if sz > 3500:
        print("(too large; refs only)")
        return
    res = di.decompileFunction(f, 120, mon)
    if res and res.decompileCompleted():
        c = res.getDecompiledFunction().getC()
        print(c[:10000])
    else:
        print("(decompile failed)")

# Decompile each writer (the smaller ones first - more likely to be the
# vertex-setter rather than a render orchestrator).
sorted_writers = sorted(seen_funcs)
for ep in sorted_writers:
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    sz = int(f.getBody().getNumAddresses())
    if sz < 2500 and sz > 0:
        show(ep, "vertex-buf user")

print("\n[done]")
