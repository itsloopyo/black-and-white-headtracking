# Disassemble the region around 0x0047AF7A/0x0047AF98 (a pair of FUN_0081b370
# callers with no Ghidra-defined function) to identify the collar/object pick.

fact = currentProgram.getAddressFactory()
lst  = currentProgram.getListing()
af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Walk backwards to find the nearest function entry / label before the pair.
start = 0x0047AE60
end   = 0x0047AFD0
print("=== disasm 0x{:08X}..0x{:08X} ===".format(start, end))
a = addr(start)
endA = addr(end)
while a.compareTo(endA) < 0:
    ins = lst.getInstructionAt(a)
    if ins is None:
        cu = lst.getCodeUnitAt(a)
        print("  0x{:08X}: (data) {}".format(a.getOffset(), cu))
        a = a.add(1)
        continue
    f = af.getFunctionContaining(a)
    fn = f.getName() if f else "-"
    print("  0x{:08X}: {:<32}  [{}]".format(a.getOffset(), ins.toString(), fn))
    a = a.add(ins.getLength())

# Who references into this region? Find the function entry that flows here.
print("\n=== references TO 0x0047AF7A and 0x0047AF98 ===")
for t in (0x0047AF7A, 0x0047AF98):
    rs = ref.getReferencesTo(addr(t))
    for r in rs:
        print("  {} -> 0x{:08X}".format(r.getFromAddress(), t))

# What function (if any) precedes 0x0047AE00?
print("\n=== nearest defined function around region ===")
for probe in (0x0047AE00, 0x0047AD00, 0x0047AC00, 0x0047A000, 0x0047B000):
    f = af.getFunctionContaining(addr(probe))
    print("  probe 0x{:08X}: {}".format(probe, f.getName() if f else "none"))
