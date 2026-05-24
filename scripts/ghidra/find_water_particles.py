# Locate water and particle render code in B&W (2001).
#
# Headless, runs from analyzeHeadless. Two passes:
#
#   1. String search: any string containing water/particle/sea/ocean/sky/
#      cloud/fog/mist/rain/smoke/fire/sprite/effect/splash/foam.
#      For each match, report functions that reference the string.
#
#   2. Camera-state reader scan: find functions that read from
#      g_cameraPivot (0x00EA1DB8), g_cameraStruct (0x00EA1D28),
#      g_scaledMatrix (0x00EA9E40), or g_mirrorMatrix (0x00EA1D58).
#      A function appearing here that ALSO matches a water/particle
#      string is a strong candidate for an independent render path.

import re

af   = currentProgram.getFunctionManager()
ref  = currentProgram.getReferenceManager()
fact = currentProgram.getAddressFactory()
sym  = currentProgram.getSymbolTable()
mem  = currentProgram.getMemory()
listing = currentProgram.getListing()

def addr(x): return fact.getAddress(hex(x).rstrip('L'))

KEYWORDS = [
    'water', 'sea', 'ocean', 'lake', 'wave', 'splash', 'foam', 'ripple',
    'particle', 'sprite', 'effect',
    'smoke', 'fire', 'flame', 'spark', 'ember',
    'sky', 'cloud', 'fog', 'mist', 'rain', 'drop', 'snow', 'dust',
]

CAMERA_ADDRS = {
    0x00EA1D28: 'g_cameraStruct',
    0x00EA1DB8: 'g_cameraPivot',
    0x00EA1DC4: 'g_cameraTo',
    0x00EA9E40: 'g_scaledMatrix',
    0x00EA1D58: 'g_mirrorMatrix',
    0x00E83A00: 'g_scaleX',
    0x00E83A04: 'g_scaleY',
}

KW_RE = re.compile('|'.join(KEYWORDS), re.IGNORECASE)

print("[1] Scanning defined data strings for keywords...")
print("=" * 78)
string_hits = []
data_it = listing.getDefinedData(True)
while data_it.hasNext():
    d = data_it.next()
    if not d.hasStringValue(): continue
    try:
        s = d.getDefaultValueRepresentation()
    except:
        continue
    if not s: continue
    if KW_RE.search(s):
        string_hits.append((d.getAddress(), s))

print("  {} matching strings".format(len(string_hits)))

# Group by referencing function
fn_to_strings = {}
for saddr, sval in string_hits:
    refs = ref.getReferencesTo(saddr)
    seen = set()
    for r in refs:
        fr = r.getFromAddress()
        f = af.getFunctionContaining(fr)
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        if ep in seen: continue
        seen.add(ep)
        fn_to_strings.setdefault(ep, []).append((str(saddr), sval))

print("")
print("Functions that reference water/particle/etc. strings:")
print("-" * 78)
for ep in sorted(fn_to_strings):
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    print("  FUN_{:08X}  size={}  strings:".format(ep, sz))
    for sa, sv in fn_to_strings[ep][:6]:
        sv_clean = sv[:60].replace('\n', ' ')
        print("    {}  {}".format(sa, sv_clean))
    if len(fn_to_strings[ep]) > 6:
        print("    ... and {} more".format(len(fn_to_strings[ep]) - 6))
print("")

print("[2] Scanning callers of camera-state addresses...")
print("=" * 78)
cam_readers = {}  # ep -> set of (name, addr)
for cam_addr, cam_name in CAMERA_ADDRS.items():
    refs = ref.getReferencesTo(addr(cam_addr))
    for r in refs:
        if not r.getReferenceType().isRead() and not r.getReferenceType().isData():
            continue
        fr = r.getFromAddress()
        f = af.getFunctionContaining(fr)
        if not f: continue
        ep = f.getEntryPoint().getOffset()
        cam_readers.setdefault(ep, set()).add(cam_name)

# Known camera-builder functions (already hooked) - filter these out.
KNOWN_BUILDERS = {0x00819920, 0x00819f50, 0x00818c60}

# Top: functions that read camera state AND match a string keyword
print("")
print("Cross-reference: camera-state readers AND keyword-matching:")
print("-" * 78)
intersection = sorted(set(cam_readers.keys()) & set(fn_to_strings.keys()))
for ep in intersection:
    if ep in KNOWN_BUILDERS: continue
    f = af.getFunctionAt(addr(ep))
    sz = int(f.getBody().getNumAddresses()) if f else 0
    print("  FUN_{:08X}  size={}".format(ep, sz))
    print("    camera reads: {}".format(sorted(cam_readers[ep])))
    for sa, sv in fn_to_strings[ep][:3]:
        print("    string {}: {}".format(sa, sv[:60].replace('\n', ' ')))
print("")

# Also report camera-state readers that are NOT the known builders
# (their tail-callers are downstream consumers; some are render paths).
print("All non-builder camera-state readers (size sorted desc):")
print("-" * 78)
items = []
for ep, names in cam_readers.items():
    if ep in KNOWN_BUILDERS: continue
    f = af.getFunctionAt(addr(ep))
    if not f: continue
    items.append((int(f.getBody().getNumAddresses()), ep, sorted(names)))
items.sort(reverse=True)
for sz, ep, names in items[:40]:
    print("  FUN_{:08X}  size={:6d}  reads: {}".format(ep, sz, names))

print("")
print("Done.")
