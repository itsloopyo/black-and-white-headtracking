# Hunt for MOV EDI, 0xEA9DE0 instructions (byte pattern BF E0 9D EA 00)
# which would set up a REP MOVSD write target. Also LEA EDI patterns.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
mem  = currentProgram.getMemory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Byte patterns:
#  BF E0 9D EA 00   = MOV EDI, 0xEA9DE0
#  BE E0 9D EA 00   = MOV ESI, 0xEA9DE0
#  B9 E0 9D EA 00   = MOV ECX, 0xEA9DE0
#  BA E0 9D EA 00   = MOV EDX, 0xEA9DE0
#  8D 3D E0 9D EA 00 = LEA EDI, [0xEA9DE0]
patterns = [
    (bytes([0xBF, 0xE0, 0x9D, 0xEA, 0x00]), 'MOV EDI, 0xEA9DE0'),
    (bytes([0xBE, 0xE0, 0x9D, 0xEA, 0x00]), 'MOV ESI, 0xEA9DE0'),
    (bytes([0x8D, 0x3D, 0xE0, 0x9D, 0xEA, 0x00]), 'LEA EDI, [0xEA9DE0]'),
    (bytes([0x8D, 0x35, 0xE0, 0x9D, 0xEA, 0x00]), 'LEA ESI, [0xEA9DE0]'),
]

print("Scanning for byte patterns referencing 0xEA9DE0 as a register target:")
print("=" * 78)
for pat, name in patterns:
    print("Pattern: {}".format(name))
    a = mem.getBlock(addr(0x00400000)).getStart()
    end = a.add(0x600000)
    found = []
    cur = a
    # crude scan: read each block
    for blk in mem.getBlocks():
        if not blk.isExecute(): continue
        if not blk.isInitialized(): continue
        s = blk.getStart().getOffset()
        e = blk.getEnd().getOffset()
        # Read whole block as bytes
        ba = bytearray()
        try:
            for off in range(0, min(e-s+1, 0x600000), 0x10000):
                chunk_len = min(0x10000, e - s + 1 - off)
                chunk = bytearray(chunk_len)
                mem.getBytes(addr(s + off), chunk)
                ba.extend(chunk)
        except Exception as ex:
            print("  (read error: {})".format(ex))
            continue
        # Find pattern
        idx = 0
        while True:
            i = ba.find(pat, idx)
            if i < 0: break
            site = s + i
            f = af.getFunctionContaining(addr(site))
            ep = f.getEntryPoint().getOffset() if f else 0
            found.append((site, ep, int(f.getBody().getNumAddresses()) if f else 0))
            idx = i + 1
    for site, ep, sz in found:
        print("  @ 0x{:08X}  in FUN_{:08X}  size={}".format(site, ep, sz))
    print("")
