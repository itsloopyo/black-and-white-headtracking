# Disassemble FUN_00817930 around the calls to FUN_0081FFF0 and around
# the g_scaledMatrix reads. Want to know:
#  - Where exactly g_scaledMatrix is read (start of matrix-mul loop?)
#  - What EDX is set to before each FUN_007faff0/fae60 call inside
#    FUN_0081FFF0 (which matrix is being multiplied into the bone matrix)
#
# Also: is shadow rendering happening before or after FUN_0054DA80?
# Find ALL functions whose call graph reaches FUN_00817930 - one of them
# is the actual frame caller.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# 1) Dump instructions around the FUN_007FAFF0 / FUN_007FAE60 calls in
#    FUN_0081FFF0 (need to know what's in EDX at call time)
print("[1] Instructions around FUN_007faff0/fae60 calls in FUN_0081FFF0:")
print("=" * 78)
f = af.getFunctionAt(addr(0x0081FFF0))
addrs = []
for a in f.getBody().getAddresses(True):
    ins = listing.getInstructionAt(a)
    if ins: addrs.append((a, ins))

# Find call sites
call_sites = []
for i, (a, ins) in enumerate(addrs):
    if ins.getMnemonicString() == 'CALL':
        flow = [str(r) for r in ins.getFlows()]
        for tgt_addr in flow:
            if '7faff0' in tgt_addr.lower() or '7fae60' in tgt_addr.lower():
                call_sites.append(i)

# Show 8 instructions before and 2 after each call site
for cs_idx in call_sites:
    print("")
    print("--- call site at index {} ---".format(cs_idx))
    lo = max(0, cs_idx - 8)
    hi = min(len(addrs), cs_idx + 3)
    for j in range(lo, hi):
        a, ins = addrs[j]
        marker = '  >>>' if j == cs_idx else '     '
        print("{}{}  {}".format(marker, a, ins))

# 2) Find the first FUN_00817930 instruction that reads g_scaledMatrix,
#    and show context.
print("")
print("[2] Context around first g_scaledMatrix read in FUN_00817930:")
print("=" * 78)
f = af.getFunctionAt(addr(0x00817930))
addrs2 = []
for a in f.getBody().getAddresses(True):
    ins = listing.getInstructionAt(a)
    if ins: addrs2.append((a, ins))

for i, (a, ins) in enumerate(addrs2):
    for op in range(ins.getNumOperands()):
        for r in ins.getOperandReferences(op):
            t = r.getToAddress().getOffset()
            if 0x00EA9E40 <= t < 0x00EA9E40 + 0x30:
                # Found first one. Show 15 around.
                lo = max(0, i - 15)
                hi = min(len(addrs2), i + 5)
                for j in range(lo, hi):
                    aa, ii = addrs2[j]
                    marker = '  >>>' if j == i else '     '
                    print("{}{}  {}".format(marker, aa, ii))
                break
        else:
            continue
        break
    else:
        continue
    break  # only first

# 3) Decompile head of FUN_00817930 (full body might be huge but head is what we need)
print("")
print("[3] Decompile head of FUN_00817930:")
print("=" * 78)
di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
f = af.getFunctionAt(addr(0x00817930))
res = di.decompileFunction(f, 60, mon)
if res and res.decompileCompleted():
    code = res.getDecompiledFunction().getC()
    # Print first 4500 chars
    print(code[:5000])
