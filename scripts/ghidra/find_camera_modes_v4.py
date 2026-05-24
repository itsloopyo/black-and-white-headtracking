# v4: memory-scan for COL refs (which sit at vtable[-1]) instead of
# relying on Ghidra's xref database.

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
sym  = currentProgram.getSymbolTable()
mem  = currentProgram.getMemory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))
def read_ptr(a):
    try: return mem.getInt(a) & 0xFFFFFFFF
    except: return None

mode_classes = {}
for s in sym.getAllSymbols(True):
    n = s.getName(True)
    if 'RTTI_Type_Descriptor' in n and 'CameraMode' in n:
        cls = n.split('::')[0]
        mode_classes[cls] = s.getAddress().getOffset()

td_to_class = {td: cls for cls, td in mode_classes.items()}
td_addrs = set(td_to_class.keys())

# Build COL map (COL+0xC == TD pointer).
print("Finding COLs...")
col_to_class = {}
for blk in mem.getBlocks():
    if not blk.isInitialized(): continue
    if blk.isExecute(): continue
    a = (blk.getStart().getOffset() + 3) & ~3
    end = blk.getEnd().getOffset()
    while a + 4 <= end:
        v = read_ptr(addr(a))
        if v in td_addrs:
            col_base = a - 0xC
            sig = read_ptr(addr(col_base))
            if sig in (0, 1):
                col_to_class[col_base] = td_to_class[v]
        a += 4
print("  {} COLs".format(len(col_to_class)))

# Now scan data for dwords equal to any COL address -> vtable[-1] location.
print("Scanning for vtable[-1] entries...")
col_addrs_set = set(col_to_class.keys())
class_vtables = {cls: [] for cls in mode_classes}
for blk in mem.getBlocks():
    if not blk.isInitialized(): continue
    if blk.isExecute(): continue
    a = (blk.getStart().getOffset() + 3) & ~3
    end = blk.getEnd().getOffset()
    while a + 4 <= end:
        v = read_ptr(addr(a))
        if v in col_addrs_set:
            cls = col_to_class[v]
            vt = a + 4
            if vt not in class_vtables[cls]:
                class_vtables[cls].append(vt)
        a += 4

print("Vtable contents:")
print("=" * 78)
for cls in sorted(mode_classes):
    print("[{}]".format(cls))
    vts = class_vtables[cls]
    if not vts:
        print("  (no vtable found)")
        print("")
        continue
    for vt in vts:
        print("  vtable @ 0x{:08X}".format(vt))
        for slot in range(12):
            a = vt + slot * 4
            ptr = read_ptr(addr(a))
            if ptr is None: break
            in_text = 0x00400000 <= ptr <= 0x00A00000
            if not in_text and ptr != 0: break
            if ptr == 0:
                print("    [+{:02X}] (null)".format(slot * 4))
                continue
            fn = af.getFunctionAt(addr(ptr))
            fname = fn.getName() if fn else "?"
            size = int(fn.getBody().getNumAddresses()) if fn else 0
            print("    [+{:02X}] -> 0x{:08X}  {}  size={}".format(slot*4, ptr, fname, size))
    print("")

print("Done. The MMB-grab handler is likely CameraModeFree's vtable[+08] or a nearby slot.")
