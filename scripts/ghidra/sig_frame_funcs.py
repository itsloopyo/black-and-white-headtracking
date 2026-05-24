# Print calling convention, parameter count/types, and prototype for the two
# functions we want to hook to add a post-world camera sandwich:
#   FUN_0054C190  world-render call in the frame loop (rotate cs at its exit)
#   FUN_00641C60  main frame loop (restore cs at its exit)
# Also FUN_0054DA80 (existing hooked world dispatcher) for reference.

fact = currentProgram.getAddressFactory()
fm = currentProgram.getFunctionManager()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

for ep in (0x0054C190, 0x00641C60, 0x0054DA80, 0x0054CF20):
    f = fm.getFunctionAt(addr(ep))
    if not f:
        print("FUN_{:08X}: no function".format(ep)); continue
    cc = f.getCallingConventionName()
    proto = f.getSignature().getPrototypeString()
    nparams = f.getParameterCount()
    print("FUN_{:08X}".format(ep))
    print("   conv     : {}".format(cc))
    print("   params   : {}".format(nparams))
    print("   prototype: {}".format(proto))
    print("   noreturn : {}".format(f.hasNoReturn()))
    print("")

print("Done.")
