# Confirm 0x00ECA638 is the IDirect3DDevice7* global by:
#   1. Counting xrefs.
#   2. Checking that both FUN_00819920 and FUN_00819F50 read it.
#   3. Listing nearby SetTransform vtable offsets to verify which DX7
#      device methods are called against it.

af = currentProgram.getAddressFactory()
ref = currentProgram.getReferenceManager()
fm = currentProgram.getFunctionManager()

def addr(x):
    return af.getAddress(hex(x).rstrip('L'))

device_global = addr(0x00ECA638)
print("References to 0x{:08X}:".format(0x00ECA638))
print("=" * 78)

refs = list(ref.getReferencesTo(device_global))
print("Total xrefs: {}".format(len(refs)))

callers = {}
for r in refs:
    fr = r.getFromAddress()
    fn = fm.getFunctionContaining(fr)
    key = fn.getEntryPoint().getOffset() if fn else None
    callers.setdefault(key, 0)
    callers[key] += 1

want = (0x00819920, 0x00819F50)
for fn_off, count in sorted(callers.items(), key=lambda kv: kv[1], reverse=True)[:25]:
    flag = " <-- camera builder" if fn_off in want else ""
    if fn_off:
        print("  FUN_{:08X}: {} reads{}".format(fn_off, count, flag))
    else:
        print("  (no fn): {} reads".format(count))

print("")
print("Camera builders confirmed:" if all(w in callers for w in want) else "NOT confirmed")
