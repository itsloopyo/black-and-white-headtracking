# Decompile FUN_0080DB30 (name-tag system parent: builds the per-object
# transform matrices at DAT_00e9fe48 and the billboard basis 0xF03140 that the
# box renderer FUN_0085AB10 projects through). Identify which camera matrix it
# multiplies object transforms by - g_cameraStruct (clean) vs g_scaledMatrix
# (rotated) - and whether it calls the inverter / FUN_00855340 basis builder.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))
di = DecompInterface(); di.openProgram(currentProgram); mon = ConsoleTaskMonitor()

NAMES = {0x00EA1D28:'g_cameraStruct', 0x00EA9E40:'g_scaledMatrix',
         0x00EA1D58:'g_mirrorMatrix', 0x00EA9DE0:'g_invScaledMatrix',
         0x00EA1DB8:'g_cameraPivot', 0x00EA1CC8:'billboard_mat',
         0x00F03140:'billboard_basis', 0x00E9FE48:'perObjMatBase',
         0x00855340:'FUN_855340_basisBuild', 0x007FB290:'FUN_invert'}

def annotate(ep):
    f = fm.getFunctionAt(addr(ep)); hits=[]
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            t=r.getToAddress().getOffset()
            if t in NAMES: hits.append((str(ins.getAddress()), NAMES[t], str(ins)))
    return hits

ep = 0x0080DB30
f = fm.getFunctionAt(addr(ep))
print("FUN_{:08X} size={}".format(ep, int(f.getBody().getNumAddresses())))
print("camera/billboard references:")
for a,n,s in annotate(ep):
    print("   {}  {:18} {}".format(a, n, s))
print("=" * 78)
res = di.decompileFunction(f, 60, mon)
if res and res.decompileCompleted():
    print(res.getDecompiledFunction().getC()[:8000])
print("\nDone.")
