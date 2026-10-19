import re, os, sys
CL = sys.argv[1]; ae = {}
for l in open('ae1170.tsv'):
    k, v = l.split(); ae[int(k)] = int(v, 16)
names = {}  # rva -> set(names)
def add(aeid, name):
    if aeid in ae and name:
        names.setdefault(ae[aeid], set()).add(name)
idpat = re.compile(r'RELOCATION_ID\(\s*(\d+)\s*,\s*(\d+)\s*\)|RelocationID\(\s*(\d+)\s*,\s*(\d+)\s*\)|VariantID\(\s*(\d+)\s*,\s*(\d+)\s*,')
fnpat = re.compile(r'^\s*(?:template\s*<[^>]*>\s*)?(?:[\w:<>,\*&\s\[\]]+?\s+)?((?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*\([^;]*$')
for root, _, files in os.walk(CL + '/src'):
    for fn in files:
        if not fn.endswith(('.cpp', '.h')): continue
        cur = None; stem = os.path.splitext(fn)[0]
        for line in open(os.path.join(root, fn), errors='ignore'):
            m = fnpat.match(line)
            if m and not line.strip().startswith(('if', 'for', 'while', 'return', 'switch', 'static REL', 'REL::', 'using')):
                cur = m.group(1)
            for g in idpat.finditer(line):
                a = g.group(2) or g.group(4) or g.group(6)
                n = cur if cur and '::' in cur else f'{stem}::{cur}'
                add(int(a), n)
for root, _, files in os.walk(CL + '/include'):
    for fn in files:
        if not fn.endswith('.h'): continue
        stem = os.path.splitext(fn)[0]; cur = None
        txt = open(os.path.join(root, fn), errors='ignore').read().split('\n')
        for line in txt:
            m = fnpat.match(line)
            if m: cur = m.group(1)
            # Offsets_VTABLE / RTTI style: inline constexpr REL::VariantID VTABLE_Foo[...]{ REL::VariantID(x, y, z) ...
            mv = re.search(r'(VTABLE_\w+|RTTI_\w+|NiRTTI_\w+)\b.*?VariantID\(\s*\d+\s*,\s*(\d+)', line)
            if mv:
                add(int(mv.group(2)), mv.group(1)); continue
            for g in idpat.finditer(line):
                a = g.group(2) or g.group(4) or g.group(6)
                add(int(a), f'{stem}::{cur}' if cur else stem)
with open('names1170.tsv', 'w') as f:
    for rva in sorted(names):
        f.write(f"{rva:x}\t{' | '.join(sorted(names[rva]))}\n")
print(len(names), 'named addresses')
