# Find the IDirect3DDevice7* global stored by the engine.
#
# Strategy: FUN_00819920 (per-frame camera builder) ends by calling
# IDirect3DDevice7::SetTransform(D3DTS_VIEW, &g_scaledMatrix). The
# emit pattern is roughly:
#
#     push offset g_scaledMatrix    ; 0x00EA9E40
#     push 2                        ; D3DTS_VIEW
#     mov ecx, [g_d3d_device]       ; <-- this global we want
#     push ecx                      ; this
#     mov eax, [ecx]                ; vtable
#     call dword ptr [eax + N]      ; SetTransform
#
# (The thiscall convention varies. Sometimes it's:
#     mov  ecx, [g_d3d_device]
#     mov  eax, [ecx]
#     push ...
#     push ...
#     call dword ptr [eax + N])
#
# Approach:
#   1. Walk the last ~80 bytes of FUN_00819920 (and FUN_00819F50 for
#      sanity).
#   2. Find any `mov reg, [imm32]` where imm32 lives in the .data
#      segment and is later used as the `this` for a vtable call
#      (call dword ptr [reg2 + imm8]).
#   3. Print every such candidate; the right one will be the same in
#      both 19920 and 19F50.

from ghidra.program.model.lang import OperandType

af = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
mem = currentProgram.getMemory()

def addr(x):
    return af.getAddress(hex(x).rstrip('L'))

def find_globals_in_tail(fn_start, fn_end_hint=None):
    fn = currentProgram.getFunctionManager().getFunctionAt(addr(fn_start))
    if fn is None:
        print("  no function at {:08X}".format(fn_start))
        return []
    body = fn.getBody()
    end_addr = body.getMaxAddress()
    start_addr = end_addr.subtract(180)  # last ~180 bytes
    if start_addr.compareTo(body.getMinAddress()) < 0:
        start_addr = body.getMinAddress()

    print("  scanning {} .. {} of FUN_{:08X}".format(start_addr, end_addr, fn_start))
    candidates = []
    instr = listing.getInstructionAt(start_addr)
    while instr is not None and instr.getAddress().compareTo(end_addr) <= 0:
        mnem = instr.getMnemonicString()
        # `mov reg, [imm32]` - operand 0 is reg, operand 1 is mem [imm32]
        if mnem == "MOV" and instr.getNumOperands() == 2:
            op_type1 = instr.getOperandType(1)
            if op_type1 & OperandType.ADDRESS and op_type1 & OperandType.SCALAR:
                # absolute-address memory operand
                refs = instr.getOperandReferences(1)
                for r in refs:
                    target = r.getToAddress()
                    if mem.contains(target):
                        block = mem.getBlock(target)
                        if block is not None and not block.isExecute():
                            candidates.append((instr.getAddress(), target, str(instr)))
        # Also catch `call dword ptr [reg+N]` style vtable calls
        if mnem == "CALL" and instr.getNumOperands() == 1:
            op_type0 = instr.getOperandType(0)
            if op_type0 & OperandType.DYNAMIC and op_type0 & OperandType.INDIRECT:
                print("    vtable-style call at {}: {}".format(instr.getAddress(), instr))
        instr = instr.getNext()
    return candidates

print("=== FUN_00819920 tail ===")
c1 = find_globals_in_tail(0x00819920)
print("")
print("=== FUN_00819F50 tail ===")
c2 = find_globals_in_tail(0x00819F50)

print("")
print("Globals loaded near tail of FUN_00819920:")
for site, tgt, txt in c1:
    print("  loaded {} at {}  ({})".format(tgt, site, txt))
print("")
print("Globals loaded near tail of FUN_00819F50:")
for site, tgt, txt in c2:
    print("  loaded {} at {}  ({})".format(tgt, site, txt))

# Common addresses (loaded in BOTH)
addrs1 = set(str(t) for _, t, _ in c1)
addrs2 = set(str(t) for _, t, _ in c2)
common = addrs1 & addrs2
print("")
print("Addresses loaded in BOTH function tails:")
for a in sorted(common):
    print("  {}".format(a))
