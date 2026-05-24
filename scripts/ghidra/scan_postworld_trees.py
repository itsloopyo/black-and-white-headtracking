# Expand the call trees (2-3 levels) of the six post-world HUD passes in the
# main frame loop FUN_00641C60, and for each reachable function report:
#   - whether it reads g_cameraStruct (clean -> drift) or g_scaledMatrix
#   - whether it references the D3D device (draws)
#   - any string references (to spot name/label/text code)
# These passes run AFTER the world-render sandwich exits, so anything here
# that projects through g_cameraStruct sees the clean camera and drifts.

fm   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

POST_WORLD = [0x0054EC80, 0x00564160, 0x005EA980, 0x0053C480, 0x0054EB40, 0x00643420]
DEVICE = 0x00ECA638
CAM = {0x00EA1D28:'g_cameraStruct', 0x00EA9E40:'g_scaledMatrix',
       0x00EA1D58:'g_mirrorMatrix', 0x00EA9DE0:'g_invScaledMatrix',
       0x00EA1DB8:'g_cameraPivot'}

def callees(ep):
    f = fm.getFunctionAt(addr(ep))
    out = set()
    if not f: return out
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            if r.getReferenceType().isCall():
                g = fm.getFunctionContaining(r.getToAddress())
                if g: out.add(g.getEntryPoint().getOffset())
    return out

def analyze(ep):
    f = fm.getFunctionAt(addr(ep))
    if not f: return (set(), False, [])
    reads=set(); dev=False; strs=[]
    for a in f.getBody().getAddresses(True):
        ins = listing.getInstructionAt(a)
        if not ins: continue
        for r in ins.getReferencesFrom():
            t=r.getToAddress().getOffset()
            if t in CAM: reads.add(CAM[t])
            if t==DEVICE: dev=True
            d=listing.getDataAt(r.getToAddress())
            if d and d.hasStringValue():
                v=d.getValue()
                if v and len(v)>2: strs.append(v)
    return (reads, dev, strs)

# BFS to depth 3
seen=set()
frontier=list(POST_WORLD)
depth=0
results={}
while frontier and depth<4:
    nxt=[]
    for ep in frontier:
        if ep in seen: continue
        seen.add(ep)
        results[ep]=analyze(ep)
        nxt.extend(callees(ep))
    frontier=nxt
    depth+=1

print("Functions in post-world trees that read g_cameraStruct (drift suspects):")
print("=" * 78)
for ep in sorted(results):
    reads, dev, strs = results[ep]
    if 'g_cameraStruct' in reads:
        print("  FUN_{:08X} draws={} reads={} strings={}".format(
            ep, dev, sorted(reads), strs[:6]))

print("")
print("All functions in post-world trees with interesting strings:")
print("=" * 78)
import re
KW = re.compile(r'name|Name|label|Label|text|Text|font|Font|villag|creature|tooltip|hud|HUD', re.I)
for ep in sorted(results):
    reads, dev, strs = results[ep]
    hit = [s for s in strs if KW.search(s)]
    if hit:
        print("  FUN_{:08X} reads={} draws={} :: {}".format(ep, sorted(reads), dev, hit[:8]))

print("\nTotal functions scanned: {}".format(len(results)))
print("Done.")
