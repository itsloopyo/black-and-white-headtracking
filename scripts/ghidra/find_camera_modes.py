# Enumerate B&W's CameraMode class hierarchy via RTTI.
#
# For each CameraMode-derived class:
#   - find the RTTI Type Descriptor
#   - find the vtable that points to it (via Complete Object Locator)
#   - list the first few vtable slots (virtual methods)
#
# The MMB-grab handler is the vtable[+8] (or thereabouts) of whichever
# CameraMode subclass owns MMB-drag behavior.
#
# Paste into Ghidra: Window -> Python.

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
list = currentProgram.getListing()
sym  = currentProgram.getSymbolTable()
mem  = currentProgram.getMemory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def read_ptr(a):
    try:
        return mem.getInt(a) & 0xFFFFFFFF
    except:
        return None

# 1. Find all symbols matching CameraMode*::RTTI_Type_Descriptor.
print("Searching for CameraMode RTTI type descriptors...")
mode_classes = {}
it = sym.getAllSymbols(True)
for s in it:
    n = s.getName(True)  # fully qualified
    if 'RTTI_Type_Descriptor' in n and 'CameraMode' in n:
        cls = n.split('::')[0]
        mode_classes[cls] = s.getAddress()

if not mode_classes:
    print("  None found. Trying just 'Camera' prefix...")
    it = sym.getAllSymbols(True)
    for s in it:
        n = s.getName(True)
        if 'RTTI_Type_Descriptor' in n and 'Camera' in n:
            cls = n.split('::')[0]
            mode_classes[cls] = s.getAddress()

print("  Found {} class(es):".format(len(mode_classes)))
for cls, a in sorted(mode_classes.items()):
    print("    {} @ {}".format(cls, a))
print("")

# 2. For each RTTI Type Descriptor, find its vtable.
#    Type Descriptor is referenced by a COL (Complete Object Locator).
#    The COL is referenced by the vtable at slot -1 (i.e., the dword
#    immediately before the first virtual function).
#
# Simpler approach: search for "RTTI_Complete_Object_Locator" symbols
# for each class, then xref-to to find vtable references.

print("Finding vtables...")
class_vtables = {}
for cls in mode_classes:
    # Look for vtable symbol directly first.
    vt_addr = None
    for s in sym.getSymbols("`vftable'"):
        ns = s.getParentNamespace()
        if ns and ns.getName() == cls:
            vt_addr = s.getAddress()
            break
    if vt_addr is None:
        # Fall back: find RTTI_Complete_Object_Locator for this class,
        # then xref-to gives vtable-1.
        for s in sym.getSymbols("RTTI_Complete_Object_Locator"):
            ns = s.getParentNamespace()
            if ns and ns.getName() == cls:
                col_addr = s.getAddress()
                # Find references TO this COL - the reference site is vtable-4
                refs = ref.getReferencesTo(col_addr)
                for r in refs:
                    if r.getReferenceType().isData():
                        # vtable starts 4 bytes after the COL pointer slot
                        vt_addr = r.getFromAddress().add(4)
                        break
                if vt_addr: break
    if vt_addr:
        class_vtables[cls] = vt_addr

for cls, vt in sorted(class_vtables.items()):
    print("  {}::`vftable' @ {}".format(cls, vt))

if not class_vtables:
    print("  No vtables resolved automatically.")
    print("  Try in Ghidra: Search -> For Strings -> 'CameraMode'")
    print("  Look at the RTTI windowsh in Listing for the related vtable addresses.")
print("")

# 3. For each vtable, dump first 8 slots.
print("Vtable contents (first 8 slots):")
print("=" * 78)
for cls, vt in sorted(class_vtables.items()):
    print("  {}:".format(cls))
    for slot in range(8):
        a = vt.add(slot * 4)
        ptr = read_ptr(a)
        if ptr is None:
            print("    [+{:02X}] (unreadable)".format(slot * 4))
            continue
        target = fact.getAddress(hex(ptr).rstrip('L'))
        fn = af.getFunctionAt(target)
        fname = fn.getName() if fn else "?"
        size = int(fn.getBody().getNumAddresses()) if fn else 0
        print("    [+{:02X}] -> {}  ({}  size={})".format(slot * 4, target, fname, size))
    print("")

print("Done.")
print("")
print("The MMB-grab tick is typically vtable[+08] of whichever CameraMode")
print("subclass handles middle-mouse drag. Decompile each [+08] entry and")
print("look for: cursor read, raycast through camera, write pivot/target.")
