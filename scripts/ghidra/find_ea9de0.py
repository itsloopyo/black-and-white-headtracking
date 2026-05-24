# 0xEA9DE0 is loaded as an address constant (MOV EDX, 0xEA9DE0) and
# passed to matrix-mul helpers. We need to find writers of this matrix.
# Scan a wider range and look for direct refs to 0xEA9DE0 and its
# member offsets (0..0x30 in 4-byte steps).

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

print("Refs to 0xEA9DE0 base + each member offset 0..0x30:")
print("=" * 78)
all_refs = {}
for off in range(0, 0x40, 4):
    t = 0x00EA9DE0 + off
    for r in ref.getReferencesTo(addr(t)):
        fr = r.getFromAddress()
        ins = listing.getInstructionAt(fr)
        if not ins: continue
        f = af.getFunctionContaining(fr)
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        rt = r.getReferenceType().getName()
        all_refs.setdefault(ep, []).append((off, rt, str(ins)))

# Sort by ep
for ep in sorted(all_refs):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    writes = sum(1 for (o, rt, i) in all_refs[ep] if 'WRITE' in rt)
    reads  = sum(1 for (o, rt, i) in all_refs[ep] if 'READ' in rt)
    other  = len(all_refs[ep]) - writes - reads
    print("  FUN_{:08X} size={:5d}  writes={} reads={} other={}".format(
        ep, sz, writes, reads, other))
    # Print a sample
    for (o, rt, i) in all_refs[ep][:3]:
        print("    +0x{:02X} {} {}".format(o, rt, i))

# Decompile likely candidates: small SETUP functions only.
print("")
print("Decompile FUN_00874850 (probable matrix builder):")
print("=" * 78)
di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
f = af.getFunctionAt(addr(0x00874850))
if f:
    res = di.decompileFunction(f, 30, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC()[:3500])
