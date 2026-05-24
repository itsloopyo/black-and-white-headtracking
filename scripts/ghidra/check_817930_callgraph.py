# Does FUN_0081FFF0 (shadow renderer) call FUN_00817930 (bone updater)
# transitively? If yes, we can gate a 817930-hook on g_inShadowRender.
# If no, the hook has to be unconditional.
#
# Also: who calls FUN_00817930 directly, and are they all inside the render
# scope (reachable from FUN_0054DA80)?

from ghidra.util.task import ConsoleTaskMonitor

af = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
mon = ConsoleTaskMonitor()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))
def fn(ep): return af.getFunctionAt(addr(ep))

# Transitive callees of a function
def reachable_from(ep, max_depth=12):
    seen = set()
    stack = [(ep, 0)]
    while stack:
        e, d = stack.pop()
        if e in seen or d > max_depth: continue
        seen.add(e)
        f = fn(e)
        if not f: continue
        for c in f.getCalledFunctions(mon):
            if c: stack.append((c.getEntryPoint().getOffset(), d + 1))
    return seen

print("Reachable callees of FUN_0081FFF0 (shadow renderer):")
sr = reachable_from(0x0081FFF0)
print("  total: {}".format(len(sr)))
print("  contains FUN_00817930? {}".format(0x00817930 in sr))
print("  contains FUN_0081C090 (projector)? {}".format(0x0081C090 in sr))

print("")
print("Reachable callees of FUN_0054DA80 (frame dispatcher):")
dr = reachable_from(0x0054DA80, max_depth=20)
print("  total: {}".format(len(dr)))
print("  contains FUN_00817930? {}".format(0x00817930 in dr))
print("  contains FUN_0081FFF0? {}".format(0x0081FFF0 in dr))

# Direct callers of 817930
print("")
print("Direct callers of FUN_00817930:")
f_817 = fn(0x00817930)
if f_817:
    for c in f_817.getCallingFunctions(mon):
        if c:
            ep = c.getEntryPoint().getOffset()
            inscope = '[in dispatcher scope]' if ep in dr else '[OUTSIDE dispatcher]'
            inshadow = '[in shadow scope]' if ep in sr else ''
            print("  FUN_{:08X}  {} {}".format(ep, inscope, inshadow))

# Is 817930 called from FUN_0081FFF0's direct callees? Or further down?
# Find the shortest path from 0081FFF0 to 00817930.
print("")
print("BFS path FUN_0081FFF0 -> FUN_00817930:")
from collections import deque
q = deque([(0x0081FFF0, [0x0081FFF0])])
seen = {0x0081FFF0}
found = None
while q:
    cur, path = q.popleft()
    if cur == 0x00817930:
        found = path
        break
    f = fn(cur)
    if not f: continue
    for c in f.getCalledFunctions(mon):
        if not c: continue
        ep = c.getEntryPoint().getOffset()
        if ep in seen: continue
        seen.add(ep)
        q.append((ep, path + [ep]))
if found:
    print("  path: " + " -> ".join("FUN_{:08X}".format(x) for x in found))
else:
    print("  NO PATH - 0081FFF0 does not (transitively) call 00817930")

# Now the other direction: does FUN_00817930 transitively call 0081C090?
# That'd mean a single bone-update call already projects through our hook.
print("")
print("BFS path FUN_00817930 -> FUN_0081C090:")
q = deque([(0x00817930, [0x00817930])])
seen = {0x00817930}
found = None
while q:
    cur, path = q.popleft()
    if cur == 0x0081C090:
        found = path
        break
    f = fn(cur)
    if not f: continue
    for c in f.getCalledFunctions(mon):
        if not c: continue
        ep = c.getEntryPoint().getOffset()
        if ep in seen: continue
        seen.add(ep)
        q.append((ep, path + [ep]))
if found:
    print("  path: " + " -> ".join("FUN_{:08X}".format(x) for x in found))
else:
    print("  NO PATH")

print("\nDone.")
