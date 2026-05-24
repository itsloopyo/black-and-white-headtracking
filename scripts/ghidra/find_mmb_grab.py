# Find MMB grab candidate functions in runblack.exe.
#
# Paste this into Ghidra: Window -> Python.
# It scores every function that reads the scaled view matrix at
# 0x00EA9E40..0x00EA9E6C by how MMB-grab-like it looks:
#   +5  calls FUN_00819920 (camera updater)
#   +5  calls FUN_00819f50 (scripted camera path)
#   +3  references GetCursorPos / SetCursorPos / DirectInput8Create
#   +2  writes to g_cameraPivot (0x00EA1DB8..0x00EA1DC4) or target (0x00EA1DC4..0x00EA1DD0)
#   +1  reads g_cameraStruct (0x00EA1D28..0x00EA1D58) - most camera-aware functions do
#   -2  is huge (>2000 bytes) - probably a generic render loop, not a click handler
# Prints top 20.

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
mem  = currentProgram.getMemory()
fact = currentProgram.getAddressFactory()
list = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

SCALED_RANGE = (0x00EA9E40, 0x00EA9E6C + 4)
PIVOT_RANGE  = (0x00EA1DB8, 0x00EA1DC4)
TARGET_RANGE = (0x00EA1DC4, 0x00EA1DD0)
STRUCT_RANGE = (0x00EA1D28, 0x00EA1D28 + 48)
FN_19920     = 0x00819920
FN_19F50     = 0x00819F50

INPUT_NAMES = ('GetCursorPos', 'SetCursorPos', 'DirectInput8Create',
               'DirectInputCreate', 'DirectInputCreateA', 'DirectInputCreateEx')

def funcs_reading_range(lo, hi, read_only=True):
    out = set()
    for a in range(lo, hi):
        for r in ref.getReferencesTo(addr(a)):
            if read_only and not r.getReferenceType().isRead():
                continue
            f = af.getFunctionContaining(r.getFromAddress())
            if f is not None:
                out.add(f.getEntryPoint().getOffset())
    return out

def funcs_writing_range(lo, hi):
    out = set()
    for a in range(lo, hi):
        for r in ref.getReferencesTo(addr(a)):
            if not r.getReferenceType().isWrite():
                continue
            f = af.getFunctionContaining(r.getFromAddress())
            if f is not None:
                out.add(f.getEntryPoint().getOffset())
    return out

def func_calls_target(func_entry, target_entry):
    fn = af.getFunctionAt(addr(func_entry))
    if fn is None: return False
    target = addr(target_entry)
    for it in fn.getBody().getAddressRanges():
        a = it.getMinAddress()
        while a is not None and a.compareTo(it.getMaxAddress()) <= 0:
            ins = list.getInstructionAt(a)
            if ins is not None:
                for r in ins.getReferencesFrom():
                    if r.getReferenceType().isCall() and r.getToAddress() == target:
                        return True
            a = a.next() if ins is None else ins.getMaxAddress().next()
    return False

def func_references_symbols(func_entry, names):
    fn = af.getFunctionAt(addr(func_entry))
    if fn is None: return False
    name_set = set(names)
    for it in fn.getBody().getAddressRanges():
        a = it.getMinAddress()
        while a is not None and a.compareTo(it.getMaxAddress()) <= 0:
            ins = list.getInstructionAt(a)
            if ins is not None:
                for r in ins.getReferencesFrom():
                    sym = currentProgram.getSymbolTable().getPrimarySymbol(r.getToAddress())
                    if sym is not None and sym.getName() in name_set:
                        return True
                a = ins.getMaxAddress().next()
            else:
                a = a.next()
    return False

def func_size(func_entry):
    fn = af.getFunctionAt(addr(func_entry))
    if fn is None: return 0
    return int(fn.getBody().getNumAddresses())

print("Collecting readers of scaled matrix 0x{:08X}..0x{:08X}...".format(*SCALED_RANGE))
candidates = funcs_reading_range(*SCALED_RANGE)
print("  {} unique readers".format(len(candidates)))

pivot_writers  = funcs_writing_range(*PIVOT_RANGE)
target_writers = funcs_writing_range(*TARGET_RANGE)
struct_readers = funcs_reading_range(*STRUCT_RANGE)

print("Scoring...")
scored = []
for fe in candidates:
    score = 0
    reasons = []
    if func_calls_target(fe, FN_19920):
        score += 5; reasons.append("calls 19920")
    if func_calls_target(fe, FN_19F50):
        score += 5; reasons.append("calls 19f50")
    if func_references_symbols(fe, INPUT_NAMES):
        score += 3; reasons.append("input refs")
    if fe in pivot_writers or fe in target_writers:
        score += 2; reasons.append("writes pivot/target")
    if fe in struct_readers:
        score += 1; reasons.append("reads g_cameraStruct")
    size = func_size(fe)
    if size > 2000:
        score -= 2; reasons.append("huge ({}B)".format(size))
    if score > 0:
        scored.append((score, fe, size, reasons))

scored.sort(key=lambda x: (-x[0], x[1]))
print("")
print("Top candidates (score, function, size, reasons):")
print("-" * 78)
for s, fe, size, reasons in scored[:20]:
    print("  {:+3d}  FUN_{:08X}  size={:5d}  {}".format(s, fe, size, ", ".join(reasons)))
print("")
print("Done. Inspect top entries; the MMB grab will:")
print("  - read cursor/DI input")
print("  - read scaled matrix (raycast)")
print("  - call FUN_00819920 (re-aim) OR write pivot/target directly")
print("  - be smallish (single-purpose handler)")
