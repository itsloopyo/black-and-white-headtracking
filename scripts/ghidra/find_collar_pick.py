# Identify which FUN_0081B370 (screen->world) callers correspond to object/
# collar selection. Return addresses captured at runtime; find the containing
# function for each and decompile it so we can spot the entity-pick logic.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

# Runtime-captured return addresses (callers of FUN_0081B370).
RET_ADDRS = [
    0x0047AF7A,
    0x0047AF98,
    0x005E5631,
    0x0068B46C,
    0x00456F9A,
]

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
lst  = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

seen = set()
for ra in RET_ADDRS:
    f = af.getFunctionContaining(addr(ra))
    if not f:
        print("=== ra 0x{:08X}: NO containing function".format(ra))
        continue
    entry = f.getEntryPoint().getOffset()
    print("=" * 78)
    print("=== ra 0x{:08X}  in {}  entry=0x{:08X}".format(ra, f.getName(), entry))
    # show a few instructions around the call site
    ins = lst.getInstructionContaining(addr(ra - 1))
    cur = lst.getInstructionAt(f.getEntryPoint())
    if entry in seen:
        print("    (function already decompiled above)")
        continue
    seen.add(entry)
    res = di.decompileFunction(f, 60, mon)
    if res is None or not res.decompileCompleted():
        print("(decompile failed)")
        continue
    print(res.getDecompiledFunction().getC())
    print("")
