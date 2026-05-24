# Enumerate B&W CameraMode vtables, v2.
#
# Type Descriptor -> COL -> vtable chain:
#   - COL is a struct containing a pointer to the Type Descriptor (at COL+0xC).
#   - vtable[-1] is a pointer to the COL.
#   - vtable[0..N] are the virtual methods.
#
# We walk: ref TO Type Descriptor identifies the COL slot. Ref TO that
# COL slot identifies vtable[-1]. vtable starts at vtable[-1] + 4.

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
it = sym.getAllSymbols(True)
for s in it:
    n = s.getName(True)
    if 'RTTI_Type_Descriptor' in n and 'CameraMode' in n:
        cls = n.split('::')[0]
        mode_classes[cls] = s.getAddress()

print("Found {} CameraMode classes".format(len(mode_classes)))
print("")

# For each class, walk xrefs to find vtables.
print("Resolving vtables via Type Descriptor xref chains...")
print("=" * 78)
for cls, td_addr in sorted(mode_classes.items()):
    print("[{}]  TD @ {}".format(cls, td_addr))
    # Step 1: find all references TO the type descriptor.
    # These should be inside COL structs at COL+0xC.
    col_candidates = []
    for r in ref.getReferencesTo(td_addr):
        if r.getReferenceType().isData():
            col_candidates.append(r.getFromAddress())

    # Step 2: for each candidate COL slot, find references TO it.
    # Those references are vtable[-1] entries.
    vtables = []
    for col_slot in col_candidates:
        # The COL "address" is col_slot - 0xC (if we point to pTypeDesc field).
        # But for finding the vtable, we want xrefs to whatever the vtable[-1]
        # points to. The vtable[-1] points to the COL base. Different binaries
        # store the COL at different alignment; try a few offsets.
        for col_offset_from_td_ref in (0xC, 0x0):
            possible_col_base = col_slot.subtract(col_offset_from_td_ref)
            for r in ref.getReferencesTo(possible_col_base):
                if r.getReferenceType().isData():
                    # This reference is vtable[-1]. vtable starts at +4.
                    vt = r.getFromAddress().add(4)
                    if vt not in vtables:
                        vtables.append(vt)

    if not vtables:
        # Last resort: search all references to addresses within +/- 32 of the TD.
        for off in range(-32, 33, 4):
            try:
                near = td_addr.add(off) if off >= 0 else td_addr.subtract(-off)
                for r in ref.getReferencesTo(near):
                    if r.getReferenceType().isData():
                        # walk back to find a vtable-shaped target
                        fr = r.getFromAddress()
                        # check if fr-4 or fr+4 is referenced from anywhere
                        for r2 in ref.getReferencesTo(fr):
                            if r2.getReferenceType().isData():
                                vt = r2.getFromAddress().add(4)
                                if vt not in vtables:
                                    vtables.append(vt)
            except:
                pass

    if not vtables:
        print("  (no vtable found)")
        print("")
        continue

    for vt in vtables:
        print("  vtable @ {}".format(vt))
        for slot in range(8):
            a = vt.add(slot * 4)
            ptr = read_ptr(a)
            if ptr is None or ptr == 0:
                print("    [+{:02X}] (null)".format(slot * 4))
                continue
            target = addr(ptr)
            fn = af.getFunctionAt(target)
            fname = fn.getName() if fn else "?"
            size = int(fn.getBody().getNumAddresses()) if fn else 0
            # Filter: code addresses should be in .text (low addresses, 0x4XX-0x9XX).
            in_text = 0x00400000 <= ptr <= 0x00A00000
            mark = "" if in_text else "  (NOT CODE - vtable end?)"
            print("    [+{:02X}] -> {}  {}  size={}{}".format(
                slot * 4, target, fname, size, mark))
            if not in_text:
                break
    print("")

print("Done.")
