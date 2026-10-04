import sys, capstone as cs
f, base, start, n = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16), int(sys.argv[4])
code = open(f, "rb").read()
md = cs.Cs(cs.CS_ARCH_M68K, cs.CS_MODE_M68K_000)
off = start - base
count = 0
while off < len(code) and count < n:
    got = False
    for i in md.disasm(code[off:off + 64], base + off, 1):
        print("%06X: %-20s %-8s %s" % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
        off += i.size; got = True
    if not got:
        print("%06X: %04x  dc.w" % (base + off, int.from_bytes(code[off:off + 2], "big")))
        off += 2
    count += 1
