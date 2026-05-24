# Enumerate all functions referencing the CitadelHeart.cpp assert filename
# (0x009ce9d0) - that is the citadel-heart subsystem. List them with size and a
# flag for whether they read g_cameraStruct (0x00EA1D28) or g_scaledMatrix
# (0x00EA9E40..) or call b370 - the pick/projection will stand out.

af   = currentProgram.getFunctionManager()
fact = currentProgram.getAddressFactory()
ref  = currentProgram.getReferenceManager()
def addr(x): return fact.getAddress(hex(x).rstrip('L'))

CIT_STR = 0x009ce9d0
funcs = {}
for r in ref.getReferencesTo(addr(CIT_STR)):
    f = af.getFunctionContaining(r.getFromAddress())
    if f:
        funcs[f.getEntryPoint().getOffset()] = f

# Which globals each function reads.
CS  = 0x00EA1D28   # g_cameraStruct (clean)
SC0 = 0x00EA9E40   # g_scaledMatrix (rotated render)
SCED = 0x00EA9E70
PIV = 0x00EA1DB8   # g_cameraPivot
B370 = 0x0081B370

def reads_range(f, lo, hi):
    body = f.getBody()
    it = currentProgram.getReferenceManager().getReferenceSourceIterator(body, True)
    for a in it:
        for r in currentProgram.getReferenceManager().getReferencesFrom(a):
            t = r.getToAddress().getOffset()
            if lo <= t <= hi:
                return True
    return False

print("=== CitadelHeart.cpp functions ({}) ===".format(len(funcs)))
for entry in sorted(funcs):
    f = funcs[entry]
    sz = int(f.getBody().getNumAddresses())
    tags = []
    if reads_range(f, CS, CS+0x2f):   tags.append("CLEAN_CS")
    if reads_range(f, SC0, SCED):     tags.append("SCALED")
    if reads_range(f, PIV, PIV+0xb):  tags.append("PIVOT")
    if reads_range(f, B370, B370):    tags.append("b370")
    print("  0x{:08X} size={:5}  {}".format(entry, sz, " ".join(tags)))
