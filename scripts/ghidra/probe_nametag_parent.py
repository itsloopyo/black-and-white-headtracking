# Decompile FUN_0080E4F0, the parent of the name-box renderers FUN_0085AB10 /
# FUN_0085BA30. It builds the billboard transform (0xEA1CC8 / 0xF03140 basis)
# they project through. Show its camera-global reads and body so we can see
# WHICH matrix (clean g_cameraStruct vs rotated g_scaledMatrix) and WHEN it
# feeds the billboard - that determines why the boxes don't track head rotation.

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))
di = DecompInterface(); di.openProgram(currentProgram); mon = ConsoleTaskMonitor()

CAM = {0x00EA1D28:'g_cameraStruct', 0x00EA9E40:'g_scaledMatrix',
       0x00EA1D58:'g_mirrorMatrix', 0x00EA9DE0:'g_invScaledMatrix',
       0x00EA1DB8:'g_cameraPivot', 0x00EA1CC8:'billboard_mat',
       0x00EA9E90:'point_EA9E90', 0x00F03140:'billboard_basis'}

def reads(ep):
    f = fm.getFunctionAt(addr(ep)); out=set()
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for op in range(ins.getNumOperands()):
            for r in ins.getOperandReferences(op):
                t=r.getToAddress().getOffset()
                if t in CAM: out.add(CAM[t])
    return out

for ep in (0x0080E4F0,):
    f = fm.getFunctionAt(addr(ep))
    print("FUN_{:08X} size={} reads={}".format(ep, int(f.getBody().getNumAddresses()), sorted(reads(ep))))
    print("callers:")
    for r in ref.getReferencesTo(addr(ep)):
        if r.getReferenceType().isCall():
            c=fm.getFunctionContaining(r.getFromAddress())
            print("   FUN_{:08X} @ {}".format(c.getEntryPoint().getOffset() if c else 0, r.getFromAddress()))
    print("=" * 78)
    res = di.decompileFunction(f, 60, mon)
    if res and res.decompileCompleted():
        print(res.getDecompiledFunction().getC()[:7000])

print("\nDone.")
