# Verify the call site of FUN_005E42E0 at 0x0054DEDE: instruction (plain CALL?),
# preceding instructions (any ECX setup that would indicate __thiscall?), and
# how the return value is used (push to register? discard?). Also dump the
# function's prologue to confirm no early ECX/EDX use.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Disasm at the call site and 6 instructions before + 4 after.
print("=== Call site 0x0054DEDE region (FUN_0054DA80 calling FUN_005E42E0) ===")
a = addr(0x0054DEDE)
# Step back 6 instructions
back = a
for _ in range(6):
    prev = listing.getInstructionBefore(back)
    if prev is None: break
    back = prev.getAddress()
cur = back
for _ in range(12):
    ins = listing.getInstructionAt(cur)
    if ins is None: break
    print("  0x{:08X}: {}".format(cur.getOffset(), ins.toString()))
    cur = cur.add(ins.getLength())

print("\n=== FUN_005E42E0 prologue (first 12 instructions) ===")
cur = addr(0x005E42E0)
for _ in range(12):
    ins = listing.getInstructionAt(cur)
    if ins is None: break
    print("  0x{:08X}: {}".format(cur.getOffset(), ins.toString()))
    cur = cur.add(ins.getLength())

print("\n=== FUN_005E42E0 size / end ===")
f = af.getFunctionAt(addr(0x005E42E0))
print("  body addresses: {}".format(int(f.getBody().getNumAddresses())))
print("  max addr in body: 0x{:08X}".format(f.getBody().getMaxAddress().getOffset()))

# Check return type and stack discipline at the end (look for ret near end)
print("\n=== Last 8 instructions ===")
end = f.getBody().getMaxAddress()
cur = end
prev_addrs = []
for _ in range(8):
    ins = listing.getInstructionBefore(cur)
    if ins is None: break
    prev_addrs.append(ins.getAddress())
    cur = ins.getAddress()
for a in reversed(prev_addrs):
    ins = listing.getInstructionAt(a)
    print("  0x{:08X}: {}".format(a.getOffset(), ins.toString()))

print("\n[done]")
