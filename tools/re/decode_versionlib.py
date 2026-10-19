import struct, sys
def decode(path):
    d = open(path, 'rb').read(); p = 0
    def rd(fmt):
        nonlocal p
        v = struct.unpack_from(fmt, d, p); p += struct.calcsize(fmt); return v[0]
    fmt = rd('<i'); assert fmt == 2, fmt
    ver = [rd('<i') for _ in range(4)]
    nl = rd('<i'); p += nl
    ptr = rd('<i'); cnt = rd('<i')
    pvid = 0; poff = 0; out = {}
    for _ in range(cnt):
        t = rd('<B'); lo = t & 0xF; hi = t >> 4
        if lo == 0: i = rd('<Q')
        elif lo == 1: i = pvid + 1
        elif lo == 2: i = pvid + rd('<B')
        elif lo == 3: i = pvid - rd('<B')
        elif lo == 4: i = pvid + rd('<H')
        elif lo == 5: i = pvid - rd('<H')
        elif lo == 6: i = rd('<H')
        elif lo == 7: i = rd('<I')
        else: raise ValueError(lo)
        tp = poff // ptr if hi & 8 else poff
        h = hi & 7
        if h == 0: o = rd('<Q')
        elif h == 1: o = tp + 1
        elif h == 2: o = tp + rd('<B')
        elif h == 3: o = tp - rd('<B')
        elif h == 4: o = tp + rd('<H')
        elif h == 5: o = tp - rd('<H')
        elif h == 6: o = rd('<H')
        elif h == 7: o = rd('<I')
        if hi & 8: o *= ptr
        out[i] = o; pvid = i; poff = o
    return ver, out
ver, m = decode(sys.argv[1])
print('version', ver, 'entries', len(m), file=sys.stderr)
with open(sys.argv[2], 'w') as f:
    for k in sorted(m): f.write(f"{k}\t{m[k]:x}\n")
