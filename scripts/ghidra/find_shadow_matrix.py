# Find writers and readers of the mystery matrix at 0x00EA9DE0.
# Hypothesis: it's an UNSCALED view matrix used for shadow transforms
# (and possibly other CPU-side things that don't want the projection
# scale applied). Hooking the matrix recomputation to use the rotated
# basis instead of clean would fix shadow drift.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# Look at the WHOLE matrix region: 0xEA9DE0..0xEA9E40 (96 bytes = 8 floats)
# and 0xEA9E40..0xEA9EA0 (g_scaledMatrix + neighbors).
print("=" * 78)
print("All refs into 0xEA9DE0..0xEA9E10 (mystery matrix block):")
print("=" * 78)
fn_reads = {}
fn_writes = {}
for offset in range(0, 0x30, 4):
    t = 0x00EA9DE0 + offset
    for r in ref.getReferencesTo(addr(t)):
        fr = r.getFromAddress()
        f = af.getFunctionContaining(fr)
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        rt = r.getReferenceType().getName()
        bucket = fn_writes if 'WRITE' in rt else fn_reads
        bucket.setdefault(ep, 0)
        bucket[ep] += 1

print("WRITERS (function -> count):")
for ep in sorted(fn_writes):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    nm = f.getName() if f else '?'
    print("  FUN_{:08X} {} size={} writes={}".format(ep, nm, sz, fn_writes[ep]))

print("")
print("READERS:")
for ep in sorted(fn_reads):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    print("  FUN_{:08X} size={} reads={}".format(ep, sz, fn_reads[ep]))

# Also check 0xEA9E10 (another "curious" target from earlier recon).
print("")
print("=" * 78)
print("Refs into 0xEA9E10..0xEA9E40 block:")
print("=" * 78)
fn_reads2 = {}
fn_writes2 = {}
for offset in range(0, 0x30, 4):
    t = 0x00EA9E10 + offset
    for r in ref.getReferencesTo(addr(t)):
        fr = r.getFromAddress()
        f = af.getFunctionContaining(fr)
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        rt = r.getReferenceType().getName()
        bucket = fn_writes2 if 'WRITE' in rt else fn_reads2
        bucket.setdefault(ep, 0)
        bucket[ep] += 1

print("WRITERS:")
for ep in sorted(fn_writes2):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    print("  FUN_{:08X} size={} writes={}".format(ep, sz, fn_writes2[ep]))
print("READERS:")
for ep in sorted(fn_reads2):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    print("  FUN_{:08X} size={} reads={}".format(ep, sz, fn_reads2[ep]))
