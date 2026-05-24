# The DrawPrimitive capture found the engine UI/overlay batch submitter at
# return address 0x0082F919 (XYZRHW|DIFFUSE|SPECULAR|TEX1 quads = name boxes,
# tooltips, sprites). Identify its containing function, decompile it, list its
# callers, and report which camera globals it (and its immediate callers) read,
# so we can find where the pre-transformed screen positions are computed.

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
       0x00EA1DB8:'g_cameraPivot', 0x00EA1DC4:'g_cameraTarget'}

def cam_reads(ep):
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

def strings_in(ep):
    f = fm.getFunctionAt(addr(ep)); out=[]
    if not f: return out
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            d = listing.getDataAt(r.getToAddress())
            if d and d.hasStringValue(): out.append(d.getValue())
    return out

site = addr(0x0082F919)
f = fm.getFunctionContaining(site)
ep = f.getEntryPoint().getOffset()
print("Submit site 0x0082F919 is in FUN_{:08X} (size={})".format(
    ep, int(f.getBody().getNumAddresses())))
print("  cam_reads = {}".format(sorted(cam_reads(ep))))
print("  strings   = {}".format(strings_in(ep)[:12]))
print("")

print("Callers of FUN_{:08X}:".format(ep))
for r in ref.getReferencesTo(addr(ep)):
    if not r.getReferenceType().isCall(): continue
    c = fm.getFunctionContaining(r.getFromAddress())
    if c:
        cep = c.getEntryPoint().getOffset()
        print("  FUN_{:08X} @ {} cam_reads={} strings={}".format(
            cep, r.getFromAddress(), sorted(cam_reads(cep)), strings_in(cep)[:8]))

print("\n" + "=" * 78)
print("Decompile FUN_{:08X}:".format(ep))
print("=" * 78)
res = di.decompileFunction(f, 60, mon)
if res and res.decompileCompleted():
    print(res.getDecompiledFunction().getC()[:6000])
print("\nDone.")
