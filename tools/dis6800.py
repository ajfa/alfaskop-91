import sys, capstone as cs
f, base = sys.argv[1], int(sys.argv[2], 16)
start = int(sys.argv[3], 16) if len(sys.argv) > 3 else base
code = open(f, "rb").read()
md = cs.Cs(cs.CS_ARCH_M680X, cs.CS_MODE_M680X_6800)
off = start - base
while off < len(code):
    got = False
    for i in md.disasm(code[off:], base + off):
        print("%04X: %-12s %-6s %s" % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
        off = i.address - base + i.size
        got = True
    if off < len(code):
        print("%04X: %02x          db" % (base + off, code[off]))
        off += 1
