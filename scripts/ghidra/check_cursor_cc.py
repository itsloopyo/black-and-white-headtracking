# Print exact calling convention + signature for the cursor-world functions
# so the detour wrapper forwards args correctly.

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

for a in (0x005E5620, 0x00800C30, 0x0057A5E0, 0x005E5740, 0x005CEAD0):
    f = af.getFunctionContaining(addr(a))
    if not f:
        print("0x{:08X}: no function".format(a)); continue
    print("0x{:08X} {}: conv={} ret={} params:".format(
        a, f.getName(), f.getCallingConventionName(), f.getReturnType()))
    for p in f.getParameters():
        print("    {} {}  ({})".format(p.getDataType(), p.getName(), p.getRegister()))
