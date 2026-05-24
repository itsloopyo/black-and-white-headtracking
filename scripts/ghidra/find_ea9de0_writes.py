# 0xEA9DE0 is loaded as ECX before FUN_007FAFF0/AE60 calls if it's the
# DESTINATION of a matrix multiply (param_1 = ECX in __fastcall convention,
# and those helpers do param_1 = param_1 * param_2). So scanning for
# MOV ECX, 0xEA9DE0 followed by CALL 0x007F* should reveal the writers.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
mem = currentProgram.getMemory()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Byte patterns:
#   B9 E0 9D EA 00  MOV ECX, 0xEA9DE0 (5 bytes)
#   8D 0D E0 9D EA 00  LEA ECX, [0xEA9DE0] (6 bytes)
patterns = [
    (bytes([0xB9, 0xE0, 0x9D, 0xEA, 0x00]), 'MOV ECX, 0xEA9DE0'),
    (bytes([0x8D, 0x0D, 0xE0, 0x9D, 0xEA, 0x00]), 'LEA ECX, [0xEA9DE0]'),
]

print("Scanning .text for MOV/LEA ECX <- 0xEA9DE0 (matrix-write destinations):")
print("=" * 78)
for pat, name in patterns:
    print("\nPattern: " + name)
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
            i = ba.find(pat, i)
            if i < 0: break
            site = s + i
            f = af.getFunctionContaining(addr(site))
            ep = f.getEntryPoint().getOffset() if f else 0
            sz = int(f.getBody().getNumAddresses()) if f else 0
            # Show 4 instructions starting here
            ins = listing.getInstructionAt(addr(site))
            ctx = []
            cur = ins
            for _ in range(5):
                if cur is None: break
                ctx.append("    {}  {}".format(cur.getAddress(), cur))
                cur = cur.getNext()
            print("  @ 0x{:08X}  in FUN_{:08X} (size={})".format(site, ep, sz))
            for line in ctx: print(line)
            i += 1

print("\nDone.")
