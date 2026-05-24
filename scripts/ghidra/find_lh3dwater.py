# Find LH3DWater's render method and the matrix it consumes.
#
# Open/Close are at 0x008792E0/0x008794A0. Render is a sibling method
# of the same class - find LH3DWater's vtable by looking for COL refs
# to functions in this neighborhood, OR enumerate all functions in the
# 0x00879XXX range and rank by D3D primitive usage / matrix reads.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
sym  = currentProgram.getSymbolTable()
mem  = currentProgram.getMemory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Camera-state addresses (and a few candidates for the 'second matrix').
WATCH = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA1DB8: 'g_cameraPivot',
    0x00EA1DC4: 'g_cameraTo',
    0x00EA9E10: 'g_secondMatrix?',  # the curious copy target
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00EA9EA0: 'g_activeMatrixPtr',
}

print("[1] All functions in 0x00879000 - 0x0087A000 (LH3DWater neighborhood)")
print("=" * 78)
start = 0x00879000
end   = 0x0087A000
funcs = []
it = af.getFunctions(True)
while it.hasNext():
    f = it.next()
    ep = f.getEntryPoint().getOffset()
    if start <= ep < end:
        funcs.append(f)

for f in sorted(funcs, key=lambda x: x.getEntryPoint().getOffset()):
    ep = f.getEntryPoint().getOffset()
    sz = int(f.getBody().getNumAddresses())
    # which camera-state addresses does this function read?
    reads = set()
    body = f.getBody()
    for a in body.getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        # check operand references
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t = r.getToAddress().getOffset()
                if t in WATCH:
                    reads.add(WATCH[t])
    rmark = ' [reads: {}]'.format(sorted(reads)) if reads else ''
    print("  FUN_{:08X}  size={:5d}{}".format(ep, sz, rmark))

print("")
print("[2] All xrefs that READ from g_secondMatrix? (0x00EA9E10):")
print("-" * 78)
for r in ref.getReferencesTo(addr(0x00EA9E10)):
    fr = r.getFromAddress()
    f = af.getFunctionContaining(fr)
    if f:
        print("  {} in FUN_{:08X}  type={}".format(fr, f.getEntryPoint().getOffset(),
                                                    r.getReferenceType().getName()))

print("")
print("[3] All xrefs that READ from g_activeMatrixPtr (0x00EA9EA0):")
print("-" * 78)
for r in ref.getReferencesTo(addr(0x00EA9EA0)):
    fr = r.getFromAddress()
    f = af.getFunctionContaining(fr)
    if f:
        print("  {} in FUN_{:08X}  type={}".format(fr, f.getEntryPoint().getOffset(),
                                                    r.getReferenceType().getName()))

# Look at vtable layout: LH3DWater is a class, its vtable lists Open
# (008792E0), Close (008794A0), and the render method. Find data that
# contains the address 0x008792E0 - that's vtable[i] for some i.
print("")
print("[4] Data that points at LH3DWater::Open (008792E0) - vtable slot:")
print("-" * 78)
target = 0x008792E0
for r in ref.getReferencesTo(addr(target)):
    fr = r.getFromAddress()
    f = af.getFunctionContaining(fr)
    print("  ref @ {}  type={}  in fn={}".format(
        fr, r.getReferenceType().getName(),
        ('FUN_{:08X}'.format(f.getEntryPoint().getOffset()) if f else '(data)')))

# Walk neighborhood for vtable - if Open is at vtable[+N], Render is
# probably at vtable[+M]. Dump nearby pointer-sized data.
print("")
print("[5] Sweep memory for a vtable containing 008792E0 - dump its 16 slots:")
print("-" * 78)
def read_dword(a):
    try: return mem.getInt(addr(a)) & 0xFFFFFFFF
    except: return None

for blk in mem.getBlocks():
    if not blk.isInitialized(): continue
    if blk.isExecute(): continue
    s = blk.getStart().getOffset()
    e = blk.getEnd().getOffset()
    a = (s + 3) & ~3
    while a + 4 <= e:
        v = read_dword(a)
        if v == 0x008792E0:
            # Found a slot. Dump 16 slots around it (8 before, 8 after).
            vt_lo = a - 8 * 4
            print("  candidate vtable around 0x{:08X}".format(a))
            for i in range(16):
                aa = vt_lo + i * 4
                vv = read_dword(aa)
                if vv is None: break
                in_text = 0x00400000 <= vv <= 0x00A00000
                marker = ' <- Open' if vv == 0x008792E0 else (' <- Close' if vv == 0x008794A0 else '')
                if in_text:
                    fn = af.getFunctionAt(addr(vv))
                    nm = fn.getName() if fn else '?'
                    sz = int(fn.getBody().getNumAddresses()) if fn else 0
                    print("    [+{:02X}] 0x{:08X} -> {}  size={}{}".format(
                        i*4, vv, nm, sz, marker))
                else:
                    print("    [+{:02X}] 0x{:08X} (non-code)".format(i*4, vv))
            print("")
        a += 4

print("Done.")
