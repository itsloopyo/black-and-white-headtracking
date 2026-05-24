# Find MMB grab candidate functions - v2.
#
# Path: enumerate callers of the two camera-builder functions
# (FUN_00819920, FUN_00819f50). Each caller is a function that
# decides where to point the camera - MMB grab is one of them,
# alongside scripted cutscenes, keyboard pan, etc.
#
# For each caller, print:
#   - entry address, size
#   - whether it reads g_cameraStruct (raycast)
#   - whether it reads/writes g_cameraPivot or target
#   - whether it references mouse/cursor APIs
#   - distinctive imports it references (helpful for ID)
#
# Paste into Ghidra: Window -> Python.

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
list = currentProgram.getListing()
sym  = currentProgram.getSymbolTable()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

FN_19920     = 0x00819920
FN_19F50     = 0x00819F50
PIVOT_RANGE  = (0x00EA1DB8, 0x00EA1DC4)
TARGET_RANGE = (0x00EA1DC4, 0x00EA1DD0)
STRUCT_RANGE = (0x00EA1D28, 0x00EA1D28 + 48)
SCALED_RANGE = (0x00EA9E40, 0x00EA9E70)

INPUT_HINTS = ('GetCursorPos', 'SetCursorPos', 'ScreenToClient', 'ClientToScreen',
               'DirectInput8Create', 'DirectInputCreate', 'DirectInputCreateA',
               'DirectInputCreateEx', 'GetKeyState', 'GetAsyncKeyState',
               'GetMessage', 'PeekMessage')

def callers_of(target_entry):
    out = set()
    for r in ref.getReferencesTo(addr(target_entry)):
        if r.getReferenceType().isCall():
            f = af.getFunctionContaining(r.getFromAddress())
            if f is not None:
                out.add(f.getEntryPoint().getOffset())
    return out

def func_size(fe):
    fn = af.getFunctionAt(addr(fe))
    return int(fn.getBody().getNumAddresses()) if fn else 0

def func_name(fe):
    fn = af.getFunctionAt(addr(fe))
    return fn.getName() if fn else "?"

def iter_instructions(fe):
    fn = af.getFunctionAt(addr(fe))
    if fn is None: return
    for rng in fn.getBody().getAddressRanges():
        a = rng.getMinAddress()
        while a is not None and a.compareTo(rng.getMaxAddress()) <= 0:
            ins = list.getInstructionAt(a)
            if ins is not None:
                yield ins
                a = ins.getMaxAddress().next()
            else:
                a = a.next()

def function_touches_range(fe, lo, hi, want_read=False, want_write=False):
    target_addrs = set(range(lo, hi))
    for ins in iter_instructions(fe):
        for r in ins.getReferencesFrom():
            to = r.getToAddress().getOffset()
            if to in target_addrs:
                t = r.getReferenceType()
                if want_read and t.isRead(): return True
                if want_write and t.isWrite(): return True
                if not want_read and not want_write: return True
    return False

def function_named_refs(fe, name_set):
    found = set()
    for ins in iter_instructions(fe):
        for r in ins.getReferencesFrom():
            s = sym.getPrimarySymbol(r.getToAddress())
            if s is not None and s.getName() in name_set:
                found.add(s.getName())
    return found

def function_distinct_imports(fe, limit=8):
    """Return distinct external/import symbols this function references."""
    imports = set()
    for ins in iter_instructions(fe):
        for r in ins.getReferencesFrom():
            s = sym.getPrimarySymbol(r.getToAddress())
            if s is None: continue
            ns = s.getParentNamespace()
            if ns is None: continue
            if ns.getName() not in ('Global', currentProgram.getName()):
                # belongs to an external library namespace
                imports.add('{}!{}'.format(ns.getName(), s.getName()))
        if len(imports) >= limit:
            break
    return imports

print("Enumerating callers of FUN_00819920 and FUN_00819f50...")
callers_19920 = callers_of(FN_19920)
callers_19f50 = callers_of(FN_19F50)
all_callers = callers_19920 | callers_19f50
print("  {} unique callers ({} call 19920, {} call 19f50)".format(
    len(all_callers), len(callers_19920), len(callers_19f50)))
print("")

INPUT_NAME_SET = set(INPUT_HINTS)

rows = []
for fe in sorted(all_callers):
    size = func_size(fe)
    name = func_name(fe)
    reads_struct  = function_touches_range(fe, *STRUCT_RANGE, want_read=True)
    reads_scaled  = function_touches_range(fe, *SCALED_RANGE, want_read=True)
    writes_pivot  = function_touches_range(fe, *PIVOT_RANGE, want_write=True)
    writes_target = function_touches_range(fe, *TARGET_RANGE, want_write=True)
    input_refs    = function_named_refs(fe, INPUT_NAME_SET)
    imports       = function_distinct_imports(fe, limit=6)
    calls_both    = (fe in callers_19920 and fe in callers_19f50)

    # MMB-grab score: small + reads input + reads camera state + (writes
    # pivot/target OR calls a camera builder, which all already do).
    score = 0
    if size < 800: score += 2
    if reads_struct: score += 1
    if reads_scaled: score += 1
    if writes_pivot or writes_target: score += 2
    if input_refs: score += 3
    if any('dinput' in i.lower() for i in imports): score += 3
    if any('user32' in i.lower() for i in imports): score += 1

    rows.append({
        'fe': fe, 'name': name, 'size': size, 'score': score,
        'reads_struct': reads_struct, 'reads_scaled': reads_scaled,
        'writes_pivot': writes_pivot, 'writes_target': writes_target,
        'calls_both': calls_both,
        'input_refs': input_refs, 'imports': imports,
    })

rows.sort(key=lambda r: (-r['score'], r['size']))

print("Caller report (sorted by MMB-grab-likeness):")
print("=" * 78)
for r in rows:
    print("{}  size={:5d}  score={:+d}".format(r['name'], r['size'], r['score']))
    flags = []
    if r['reads_struct']: flags.append('readsStruct')
    if r['reads_scaled']: flags.append('readsScaled')
    if r['writes_pivot']: flags.append('writesPivot')
    if r['writes_target']: flags.append('writesTarget')
    if r['calls_both']: flags.append('callsBoth')
    print("  flags: {}".format(", ".join(flags) if flags else "(none)"))
    if r['input_refs']:
        print("  input APIs: {}".format(", ".join(sorted(r['input_refs']))))
    if r['imports']:
        print("  imports: {}".format(", ".join(sorted(r['imports']))))
    print("")

print("Done.")
