# Find code paths that call into CameraMode vtable slot +0x28.
#
# These are the input/event dispatchers. One of them is what transitions
# CameraModeFree from "rest" to "MMB drag" and back, which is the source
# of the press/release jump.

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
sym  = currentProgram.getSymbolTable()
mem  = currentProgram.getMemory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

# All CameraMode vtable[+28] targets from v4 output:
SLOT28_TARGETS = [
    0x0044A6C0, 0x0044A920, 0x0044ACD0, 0x0044B4F0, 0x0044CFE0,  # Citadel,CtrInteract,Dance,FlyClick,Follow
    0x0044A3A0,  # FollowHeading -> shared base
    0x0044EA60,  # CameraModeFree
    0x007C60DB, 0x00451430, 0x004547A0, 0x00458220,  # New, New1, New2, New3
    0x00460FE0, 0x0044CFE0,  # Path, Script (shares Follow's)
    0x00462330,  # TwoObjects
]

print("Finding callers of vtable[+28] targets...")
all_callers = set()
for target in SLOT28_TARGETS:
    target_addr = addr(target)
    for r in ref.getReferencesTo(target_addr):
        if r.getReferenceType().isCall():
            f = af.getFunctionContaining(r.getFromAddress())
            if f:
                all_callers.add(f.getEntryPoint().getOffset())

print("  {} direct callers".format(len(all_callers)))
for c in sorted(all_callers):
    fn = af.getFunctionAt(addr(c))
    sz = int(fn.getBody().getNumAddresses()) if fn else 0
    nm = fn.getName() if fn else "?"
    print("    {}  size={}".format(nm, sz))
print("")

# These targets are reached only via vtable indirect-call, so direct refs
# may be sparse or empty. Scan for the indirect-call pattern instead:
#   CALL DWORD PTR [reg + 0x28]
# That's typically 3 bytes: FF 50 28
print("Scanning for indirect-call 'CALL [reg+0x28]' instruction patterns...")
candidates = set()
for blk in mem.getBlocks():
    if not blk.isInitialized(): continue
    if not blk.isExecute(): continue
    a = blk.getStart()
    end = blk.getEnd()
    while a is not None and a.compareTo(end) <= 0:
        ins = listing.getInstructionAt(a)
        if ins is not None:
            mnem = ins.getMnemonicString()
            if mnem == 'CALL' and ins.getNumOperands() >= 1:
                op_str = ins.getDefaultOperandRepresentation(0)
                # Looking for [reg + 0x28] or [reg + 28h] or [REG + 28]
                if '0x28' in op_str or '+28]' in op_str or '+ 28]' in op_str:
                    f = af.getFunctionContaining(a)
                    if f:
                        candidates.add(f.getEntryPoint().getOffset())
            a = ins.getMaxAddress().next()
        else:
            a = a.next()

print("  {} functions contain a CALL [reg+0x28]:".format(len(candidates)))
for c in sorted(candidates):
    fn = af.getFunctionAt(addr(c))
    sz = int(fn.getBody().getNumAddresses()) if fn else 0
    nm = fn.getName() if fn else "?"
    print("    {}  size={}".format(nm, sz))
print("")

# Also try +0x08 dispatch sites (Update method) for cross-reference;
# the dispatcher of +28 is often in a sibling function to +08 dispatch.
print("(Reference: known +0x08 dispatch site is at FUN_00441f80)")
print("Done.")
