# All occurrences of the 4-byte literal 0xEA9DE0 across .text. The byte
# sequence E0 9D EA 00 appears in many instruction forms; we capture them
# all and classify by the preceding opcode byte to learn how the address
# is used (push, lea, mov, etc.).

from ghidra.util.task import ConsoleTaskMonitor

mem = currentProgram.getMemory()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
af = currentProgram.getFunctionManager()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

target_bytes = bytes([0xE0, 0x9D, 0xEA, 0x00])

opcodes_seen = {}

for blk in mem.getBlocks():
    if not blk.isExecute(): continue
    if not blk.isInitialized(): continue
    s = blk.getStart().getOffset()
    e = blk.getEnd().getOffset()
    ba = bytearray()
    for off in range(0, e - s + 1, 0x10000):
        n = min(0x10000, e - s + 1 - off)
        chunk = bytearray(n)
        try:
            mem.getBytes(addr(s + off), chunk)
            ba.extend(chunk)
        except: break
    i = 0
    while True:
        i = ba.find(target_bytes, i)
        if i < 0: break
        site = s + i
        # Find instruction containing this address
        ins = listing.getInstructionContaining(addr(site))
        if ins:
            mn = ins.toString()
            f = af.getFunctionContaining(ins.getAddress())
            ep = f.getEntryPoint().getOffset() if f else 0
            opcodes_seen.setdefault(mn[:40], []).append((site, ep, ins.getAddress().getOffset()))
        i += 1

# Print grouped
print("Distinct instructions referencing 0xEA9DE0:")
print("=" * 78)
for mn in sorted(opcodes_seen):
    sites = opcodes_seen[mn]
    print("\n{} ({} occurrences)".format(mn, len(sites)))
    for site, ep, ins_at in sites[:20]:
        print("  byte@0x{:08X}  in FUN_{:08X}  instr@0x{:08X}".format(
            site, ep, ins_at))
