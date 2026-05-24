# Disassemble prologue/epilogue of FUN_005e5620 + FUN_00800c30 and a call site
# to nail down calling convention (ret vs ret N, ECX/EDX vs stack args).

fact = currentProgram.getAddressFactory()
lst  = currentProgram.getListing()
af   = currentProgram.getFunctionManager()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def dis(start, count, label):
    print("--- {} @ 0x{:08X} ---".format(label, start))
    a = addr(start)
    for _ in range(count):
        ins = lst.getInstructionAt(a)
        if ins is None:
            print("  0x{:08X}: (no instr)".format(a.getOffset())); break
        print("  0x{:08X}: {}".format(a.getOffset(), ins.toString()))
        a = a.add(ins.getLength())

def dis_func(entry, label):
    f = af.getFunctionContaining(addr(entry))
    if not f:
        print("{}: no func".format(label)); return
    body = f.getBody()
    maxa = body.getMaxAddress().getOffset()
    print("=== {} entry=0x{:08X} max=0x{:08X} ===".format(label, entry, maxa))
    dis(entry, 6, label + " prologue")
    # find the ret near the end
    dis(maxa - 6, 6, label + " tail")

dis_func(0x005E5620, "FUN_005e5620")
dis_func(0x00800C30, "FUN_00800c30")
print()
dis(0x0057A6E0, 24, "call-site to FUN_005e5620 (around 0x0057A710)")
