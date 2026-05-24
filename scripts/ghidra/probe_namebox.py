# Decompile the name-box renderer candidates and trace their projection.
# Capture matched the upper-middle name box to caller 0x0085B23D (in
# FUN_0085AB10), drawn once per villager. Decompile it + its sibling
# FUN_0085BA30, report camera-global reads, callers, and the function bodies
# so we can see which matrix projects the box's world anchor to screen.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

CAM = {0x00EA1D28:'g_cameraStruct', 0x00EA9E40:'g_scaledMatrix',
       0x00EA1D58:'g_mirrorMatrix', 0x00EA9DE0:'g_invScaledMatrix',
       0x00EA1DB8:'g_cameraPivot', 0x00EA1DC4:'g_cameraTarget',
       0x00EA1CC8:'billboard_mat', 0x00F03140:'billboard_basis'}

def reads(ep):
    f = fm.getFunctionAt(addr(ep))
    if not f: return set()
    out=set()
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t=r.getToAddress().getOffset()
                if t in CAM: out.add(CAM[t])
    return out

def callers(ep):
    out=[]
    for r in ref.getReferencesTo(addr(ep)):
        if r.getReferenceType().isCall():
            c=fm.getFunctionContaining(r.getFromAddress())
            if c: out.append((c.getEntryPoint().getOffset(), str(r.getFromAddress())))
    return out

for site, ep_hint in ((0x0085B23D, 0x0085AB10), (0x0085BEF7, 0x0085BA30)):
    f = fm.getFunctionContaining(addr(site))
    ep = f.getEntryPoint().getOffset()
    print("=" * 78)
    print("submit caller {} -> FUN_{:08X} size={} reads={}".format(
        hex(site), ep, int(f.getBody().getNumAddresses()), sorted(reads(ep))))
    print("  callers: {}".format(["FUN_{:08X}@{}".format(c,a) for c,a in callers(ep)]))
    print("=" * 78)
    res = di.decompileFunction(f, 60, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC()[:5500])
    print("")

print("Done.")
