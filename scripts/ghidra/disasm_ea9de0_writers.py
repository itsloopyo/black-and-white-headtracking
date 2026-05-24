from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
mon = ConsoleTaskMonitor()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Show 60 instructions around each site where ECX is loaded with 0xEA9DE0
# in the camera-builder functions, plus the start of the called function.
sites = [
    (0x00819920, 0x00819ba6, "FUN_00819920 (gameplay camera builder)"),
    (0x00819F50, 0x0081a308, "FUN_00819F50 (cutscene camera builder)"),
    (0x00818C60, 0x00818c9e, "FUN_00818C60 (third writer site)"),
]

for ep, site, label in sites:
    print("=" * 78)
    print("{}  (ep=0x{:08X})".format(label, ep))
    print("  context around MOV ECX, 0xEA9DE0 @ 0x{:08X}".format(site))
    print("=" * 78)
    cur = listing.getInstructionAt(addr(site - 0x40))
    count = 0
    while cur is not None and count < 40:
        ins_addr = cur.getAddress().getOffset()
        marker = "  >> " if ins_addr == site else "     "
        print("{}{:08X}  {}".format(marker, ins_addr, cur))
        cur = cur.getNext()
        count += 1
    print()

# Decompile FUN_00818C60 and FUN_0081D790 (the reader that loads all 12 floats).
print("=" * 78)
print("DECOMPILE FUN_00818C60 (potential writer entry)")
print("=" * 78)
dec = DecompInterface()
dec.openProgram(currentProgram)
for ep in [0x00818C60, 0x0081D790]:
    f = af.getFunctionAt(addr(ep))
    if not f:
        print("  no function at 0x{:08X}".format(ep))
        continue
    print("--- FUN_{:08X} signature: {}".format(ep, f.getPrototypeString(False, False)))
    r = dec.decompileFunction(f, 180, mon)
    if r and r.getDecompiledFunction():
        src = r.getDecompiledFunction().getC()
        for ln in src.splitlines()[:100]:
            print(ln)
    print()
