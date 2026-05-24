# Enumerate B&W CameraMode vtables, v3.
#
# v2 relied on Ghidra's RTTI cross-references; not all of them are present
# in this binary. v3 does a raw memory scan to find Complete Object Locators
# directly.
#
# COL layout (32-bit PE, MSVC):
#   COL+0x00: signature (0)
#   COL+0x04: offset
#   COL+0x08: cdOffset
#   COL+0x0C: pointer to Type Descriptor
#   COL+0x10: pointer to Class Hierarchy Descriptor
#
# vtable layout:
#   vtable[-1]: pointer to COL
#   vtable[+0..N]: virtual method pointers

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
sym  = currentProgram.getSymbolTable()
mem  = currentProgram.getMemory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def read_ptr(a):
    try:
        return mem.getInt(a) & 0xFFFFFFFF
    except:
        return None

# Collect CameraMode RTTI Type Descriptors.
mode_classes = {}
for s in sym.getAllSymbols(True):
    n = s.getName(True)
    if 'RTTI_Type_Descriptor' in n and 'CameraMode' in n:
        cls = n.split('::')[0]
        mode_classes[cls] = s.getAddress().getOffset()

print("Found {} CameraMode classes".format(len(mode_classes)))
print("Scanning data sections for Complete Object Locators...")

# Build set of TD addresses for fast lookup.
td_to_class = {td: cls for cls, td in mode_classes.items()}
td_addrs = set(td_to_class.keys())

# Scan all initialized data blocks. We're looking for any dword that
# matches a known TD pointer; that dword sits at COL+0xC.
col_addrs = {}  # class -> list of COL base addresses
for cls in mode_classes:
    col_addrs[cls] = []

blocks = mem.getBlocks()
for blk in blocks:
    if not blk.isInitialized(): continue
    if blk.isExecute(): continue  # skip code
    start = blk.getStart().getOffset()
    end   = blk.getEnd().getOffset()
    a = (start + 3) & ~3  # align to 4
    while a + 4 <= end:
        try:
            v = mem.getInt(addr(a)) & 0xFFFFFFFF
        except:
            a += 4
            continue
        if v in td_addrs:
            # Treat 'a' as COL+0xC.
            col_base = a - 0xC
            # Sanity: signature at COL+0x00 should be 0 or 1.
            try:
                sig = mem.getInt(addr(col_base)) & 0xFFFFFFFF
            except:
                a += 4
                continue
            if sig in (0, 1):
                col_addrs[td_to_class[v]].append(col_base)
        a += 4

# Now, for each COL, find xrefs to it. Those xrefs are vtable[-1].
print("Resolving vtables...")
print("=" * 78)
for cls in sorted(mode_classes):
    print("[{}]  TD @ 0x{:08X}".format(cls, mode_classes[cls]))
    cols = col_addrs[cls]
    if not cols:
        print("  (no COL found)")
        print("")
        continue
    seen_vts = set()
    for col_base in cols:
        col_a = addr(col_base)
        for r in ref.getReferencesTo(col_a):
            if r.getReferenceType().isData():
                vt_minus1 = r.getFromAddress()
                vt = vt_minus1.add(4)
                key = vt.getOffset()
                if key in seen_vts: continue
                seen_vts.add(key)
                print("  COL @ 0x{:08X}  ->  vtable @ {}".format(col_base, vt))
                for slot in range(10):
                    a = vt.add(slot * 4)
                    ptr = read_ptr(a)
                    if ptr is None: break
                    if ptr == 0:
                        print("    [+{:02X}] (null)".format(slot * 4))
                        continue
                    in_text = 0x00400000 <= ptr <= 0x00A00000
                    if not in_text:
                        # End of vtable (next thing isn't a code pointer).
                        break
                    fn = af.getFunctionAt(addr(ptr))
                    fname = fn.getName() if fn else "?"
                    size = int(fn.getBody().getNumAddresses()) if fn else 0
                    print("    [+{:02X}] -> 0x{:08X}  {}  size={}".format(
                        slot * 4, ptr, fname, size))
    if not seen_vts:
        # If we got COLs but no xrefs, dump raw COL info instead.
        for col_base in cols:
            print("  COL @ 0x{:08X} (no xrefs - vtable may not be analyzed)".format(col_base))
    print("")

print("Done.")
