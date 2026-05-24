# Locate where FUN_00815A70 / FUN_00817930 appear as data (vtable slots),
# then identify the containing class vtable and what's around them.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
mem  = currentProgram.getMemory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def read_dword(a):
    try: return mem.getInt(addr(a)) & 0xFFFFFFFF
    except: return None

TARGETS = [0x00815A70, 0x00817930]

for tgt in TARGETS:
    print("=" * 78)
    print("Scanning for data containing 0x{:08X}".format(tgt))
    print("=" * 78)
    for blk in mem.getBlocks():
        if not blk.isInitialized(): continue
        if blk.isExecute(): continue
        s = blk.getStart().getOffset()
        e = blk.getEnd().getOffset()
        a = (s + 3) & ~3
        while a + 4 <= e:
            v = read_dword(a)
            if v == tgt:
                # Dump 16 slots before and after.
                vt_lo = a - 8 * 4
                print("  candidate vtable around 0x{:08X}:".format(a))
                for i in range(20):
                    aa = vt_lo + i * 4
                    vv = read_dword(aa)
                    if vv is None: break
                    in_text = 0x00400000 <= vv <= 0x00A00000
                    mark = ' <- TARGET' if vv == tgt else ''
                    if in_text:
                        fn = af.getFunctionAt(addr(vv))
                        nm = fn.getName() if fn else '?'
                        sz = int(fn.getBody().getNumAddresses()) if fn else 0
                        print("    [+{:03X}] 0x{:08X} -> {}  size={}{}".format(
                            i*4, vv, nm, sz, mark))
                    else:
                        print("    [+{:03X}] 0x{:08X} (non-code)".format(i*4, vv))
                print("")
            a += 4

# Look at xrefs to PTR_DAT_00c37d9c (bone matrix table referenced in
# shadow code) to find who fills it.
print("=" * 78)
print("Xrefs to PTR_DAT_00c37d9c (0x00c37d9c) - bone matrix base:")
print("-" * 78)
for r in ref.getReferencesTo(addr(0x00C37D9C)):
    fr = r.getFromAddress()
    f = af.getFunctionContaining(fr)
    if f:
        ep = f.getEntryPoint().getOffset()
        sz = int(f.getBody().getNumAddresses())
        print("  {}  in FUN_{:08X}  size={}  type={}".format(
            fr, ep, sz, r.getReferenceType().getName()))
