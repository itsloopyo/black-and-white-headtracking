# Creature mouseover-pick works after the cursor shift, scroll mouseover does
# not. That means scroll picks read cursor from a different source than
# DAT_00ea1ac8/cc, OR project through a different matrix than g_scaledMatrix.
#
# Strategy:
#  1. Find strings containing "scroll" / "Scroll" - they appear in the binary
#     for asset loading, mission script names, debug logs.
#  2. List functions that reference those strings - those handle scroll objects.
#  3. For each, check if it reads kCursorX/Y AND projects via FUN_00819390 (the
#     same screen projector the creature pick uses) - if it does, the bug is
#     about WHICH cursor source. If it doesn't, scrolls have a custom pick.
#  4. Also list all functions that read kCursorX/Y but are NOT in the known
#     pick set - those are candidates for the scroll path.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.program.model.address import AddressSet
from ghidra.program.model.symbol import RefType

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()
mem  = currentProgram.getMemory()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

print("=== Strings containing 'scroll' (case-insensitive) ===")
# Walk defined data, look for ASCII strings.
from ghidra.program.model.data import StringDataInstance
scroll_strings = []
ditr = listing.getDefinedData(True)
count = 0
while ditr.hasNext() and count < 200:
    d = ditr.next()
    if d is None: continue
    dt = d.getDataType()
    dt_name = dt.getName().lower() if dt else ""
    if "string" not in dt_name and "char" not in dt_name and "ds" not in dt_name:
        continue
    try:
        s = d.getValue()
        if s is None: continue
        s = str(s)
        if "scroll" in s.lower() and len(s) >= 4 and len(s) < 200:
            scroll_strings.append((d.getAddress().getOffset(), s))
            count += 1
    except Exception:
        pass

print("  found {} strings".format(len(scroll_strings)))
for a, s in scroll_strings[:40]:
    print("  0x{:08X}: {!r}".format(a, s))

print("\n=== Functions referencing scroll-related strings ===")
scroll_funcs = set()
for a, s in scroll_strings:
    for r in ref.getReferencesTo(addr(a)):
        f = af.getFunctionContaining(r.getFromAddress())
        if f:
            scroll_funcs.add(f.getEntryPoint().getOffset())
print("  {} unique functions reference scroll strings".format(len(scroll_funcs)))

CURSOR_X = 0x00E852C0
CURSOR_Y = 0x00E852C4
SNAP_X = 0x00EA1AC8
SNAP_Y = 0x00EA1ACC
PROJ = 0x00819390
PROJ_B = 0x008190D0
S2W = 0x0081B370
OBJ_PICK = 0x00519960

def function_calls(func, target):
    body = func.getBody()
    tgt = addr(target)
    for a in body.getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            if not r.getReferenceType().isCall(): continue
            if r.getToAddress() == tgt:
                return True
    return False

def function_reads(func, target):
    body = func.getBody()
    tgt = addr(target)
    for a in body.getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            if r.getToAddress() == tgt:
                return True
    return False

print("\n=== Scroll-string functions and what they touch ===")
for ep in sorted(scroll_funcs)[:30]:
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    sz = int(f.getBody().getNumAddresses())
    flags = []
    if function_reads(f, CURSOR_X) or function_reads(f, CURSOR_Y): flags.append("CURSOR")
    if function_reads(f, SNAP_X) or function_reads(f, SNAP_Y): flags.append("SNAP")
    if function_calls(f, PROJ) or function_calls(f, PROJ_B): flags.append("PROJ")
    if function_calls(f, S2W): flags.append("S2W")
    if function_calls(f, OBJ_PICK): flags.append("OBJPICK")
    if flags:
        print("  FUN_{:08X} size={:5d} [{}]".format(ep, sz, " ".join(flags)))

# Now: full list of functions that read kCursorX/Y - the scroll pick must be
# in this set (or it must read SNAP). Filter to NOT in the known orchestrator
# set so we can spot the outlier.
known_pick = {0x00466730, 0x00519AD0, 0x005BC0A0, 0x005E42E0, 0x00542A90,
              0x00682F30, 0x005FFDC0, 0x0045A960, 0x005F89F0, 0x00809E50,
              0x005C83D0, 0x00576F20, 0x005E5830, 0x0044F810, 0x00570350}

print("\n=== All kCursorX readers (excluding known pick funcs) ===")
seen = set()
for r in ref.getReferencesTo(addr(CURSOR_X)):
    f = af.getFunctionContaining(r.getFromAddress())
    if not f: continue
    ep = f.getEntryPoint().getOffset()
    if ep in seen or ep in known_pick: continue
    seen.add(ep)
    sz = int(f.getBody().getNumAddresses())
    print("  FUN_{:08X} size={:5d} {}".format(ep, sz, f.getName()))

print("\n=== All DAT_00EA1AC8 (snap) readers ===")
for r in ref.getReferencesTo(addr(SNAP_X)):
    f = af.getFunctionContaining(r.getFromAddress())
    if not f: continue
    ep = f.getEntryPoint().getOffset()
    is_write = r.getReferenceType().isWrite()
    sz = int(f.getBody().getNumAddresses())
    print("  FUN_{:08X} size={:5d} {} {}".format(
        ep, sz, "WRITE" if is_write else "READ ", f.getName()))

print("\n[done]")
