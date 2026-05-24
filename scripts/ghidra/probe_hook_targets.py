# Determine arg/convention for FUN_0054C190 and FUN_00641C60 by:
#  - disassembling the first ~16 instructions (does it read [ESP+4] stack args,
#    or use ECX as 'this'?)
#  - disassembling ~8 instructions before each call site (are args PUSHed? is
#    ECX loaded?)
# This tells us whether a __fastcall(ecx) passthrough hook is safe.

fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
fm = currentProgram.getFunctionManager()
ref = currentProgram.getReferenceManager()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

def prologue(ep, n=16):
    f = fm.getFunctionAt(addr(ep))
    print("--- FUN_{:08X} prologue ---".format(ep))
    ins = listing.getInstructionAt(addr(ep))
    for _ in range(n):
        if not ins: break
        print("   {}  {}".format(ins.getAddress(), ins))
        ins = ins.getNext()

def callsites(ep, n_before=8):
    print("--- call sites of FUN_{:08X} ---".format(ep))
    for r in ref.getReferencesTo(addr(ep)):
        if not r.getReferenceType().isCall(): continue
        site = r.getFromAddress()
        cont = fm.getFunctionContaining(site)
        cn = "FUN_{:08X}".format(cont.getEntryPoint().getOffset()) if cont else "?"
        print("  call @ {} (in {})".format(site, cn))
        ins = listing.getInstructionAt(site)
        seq=[]; p=ins
        for _ in range(n_before):
            p=p.getPrevious()
            if not p: break
            seq.append(p)
        for p in reversed(seq):
            print("      {}  {}".format(p.getAddress(), p))
        print("      {}  {}  <== CALL".format(ins.getAddress(), ins))

for ep in (0x0054C190, 0x00641C60):
    prologue(ep)
    callsites(ep)
    print("")

print("Done.")
