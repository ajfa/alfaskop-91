"""Relabel the alfaskop_s41_kb INPUT_PORTS from an Ericsson keyboard table.

  kbports.py <table.bin> <alfaskop_s41_kb.cpp> [--write]

Each PORT_BIT line ends with a comment giving the code the keyboard firmware
reports for that matrix position.  That code indexes the Ericsson keyboard
table (plane 0 unshifted, plane 1 shifted, two bytes per entry: kind, code).
"""
import re, sys

tab = open(sys.argv[1], "rb").read()
src = open(sys.argv[2]).read()
write = "--write" in sys.argv

def ent(plane, i):
    o = plane * 256 + i * 2
    return tab[o], tab[o + 1]

PUNCT = {'-': "MINUS", '=': "EQUALS", ',': "COMMA", '.': "STOP", '/': "SLASH", ';': "COLON",
         "'": "QUOTE", '\\': "BACKSLASH", '<': "BACKSLASH2", '+': "EQUALS", '*': "ASTERISK",
         '@': "OPENBRACE", '[': "OPENBRACE", ']': "CLOSEBRACE", '^': "CLOSEBRACE", '%': "TILDE"}
NATIONAL = {0xe5: ("OPENBRACE", 0x00e5, 0x00c5), 0xe3: ("QUOTE", 0x00e4, 0x00c4),
            0xf8: ("COLON", 0x00f6, 0x00d6), 0xf3: ("COLON", 0x00f6, 0x00d6),
            0xe7: ("CLOSEBRACE", 0x00fc, 0x00dc)}
MOD = {0x0201: ("Shift", "LSHIFT", "UCHAR_SHIFT_1")}
FUNC = {0x420D: ("Enter", "ENTER", "13"), 0x4209: ("Tab", "TAB", "9"), 0x4208: ("Backspace", "BACKSPACE", "8")}
PF = {0x43C1 + i: "F%d" % (i + 1) for i in range(9)}
PF[0x434A] = "F10"
PF[0x434B] = "F11"
PF[0x434C] = "F12"

used = set()
def keycode_for(code):
    k0, c0 = ent(0, code)
    k1, c1 = ent(1, code)
    w0, w1 = (k0 << 8) | c0, (k1 << 8) | c1
    if k0 == 0x28 and 0x61 <= c0 <= 0x7a:
        return ("%s" % chr(c0).upper(), "KEYCODE_%s" % chr(c0).upper(), "'%s'" % chr(c0), "'%s'" % chr(c1) if k1 == 0x28 else None)
    if k0 == 0x28 and c0 in NATIONAL:
        kc, lo, up = NATIONAL[c0]
        return (chr(lo), "KEYCODE_" + kc, "0x%04x" % lo, "0x%04x" % up)
    if k0 == 0x30 and 0x30 <= c0 <= 0x39:
        d = chr(c0)
        if w1 == w0:
            return ("KP " + d, "KEYCODE_%s_PAD" % d, "'%s'" % d, None)
        sh = chr(c1) if 0x20 <= c1 < 0x7f else None
        return (d, "KEYCODE_%s" % d, "'%s'" % d, ("'%s'" % sh.replace("'", "\\'")) if sh else None)
    if k0 in (0x20, 0x28, 0x38) and 0x20 <= c0 < 0x7f:
        ch = chr(c0)
        if ch == ' ':
            return ("Space", "KEYCODE_SPACE", "' '", None)
        kc = PUNCT.get(ch)
        sh = chr(c1) if (k1 in (0x20, 0x28, 0x38) and 0x20 <= c1 < 0x7f) else None
        esc = lambda s: "'\\''" if s == "'" else ("'\\\\'" if s == "\\" else "'%s'" % s)
        if k0 == 0x38:
            return ("KP " + ch, {"-": "KEYCODE_MINUS_PAD", "+": "KEYCODE_PLUS_PAD", "*": "KEYCODE_ASTERISK"}.get(ch, "KEYCODE_" + (kc or "TILDE")), esc(ch), None)
        return (ch if not sh else ch + " " + sh, "KEYCODE_" + (kc or "TILDE"), esc(ch), esc(sh) if sh else None)
    if w0 in MOD:
        n, kc, chr_ = MOD[w0]
        return (n, "KEYCODE_" + kc, chr_, None)
    if w0 in FUNC:
        n, kc, chr_ = FUNC[w0]
        return (n, "KEYCODE_" + kc, chr_, None)
    if w0 in PF:
        f = PF[w0]
        return ("PF" + f[1:], "KEYCODE_" + f, "UCHAR_MAMEKEY(%s)" % f, None)
    return ("Key %d (%04X)" % (code, w0), None, None, None)

line_re = re.compile(r"^(\tPORT_BIT\( (0x[0-9a-f]{4}), IP_ACTIVE_LOW, IPT_KEYBOARD \)).*//\s*(\d+)(.*)$")
out, report = [], []
shift_seen = 0
for line in src.split("\n"):
    m = line_re.match(line)
    if not m:
        out.append(line)
        continue
    head, mask, code, tail = m.group(1), m.group(2), int(m.group(3)), m.group(4)
    name, kc, c_lo, c_hi = keycode_for(code)
    if kc == "KEYCODE_LSHIFT":
        if shift_seen:
            kc = "KEYCODE_RSHIFT"
        shift_seen += 1
    if kc in used and kc and not kc.endswith("SHIFT"):
        kc = None
    if kc:
        used.add(kc)
    parts = [head, ' PORT_NAME("%s")' % name.replace('"', '\\"')]
    if kc:
        parts.append(" PORT_CODE(%s)" % kc)
    if c_lo:
        parts.append(" PORT_CHAR(%s)" % c_lo)
    if c_hi:
        parts.append(" PORT_CHAR(%s)" % c_hi)
    parts.append(" // %02d%s" % (code, tail))
    out.append("".join(parts))
    report.append("%3d %-22s %-22s %s %s" % (code, name, kc or "-", c_lo or "", c_hi or ""))

for r in sorted(report, key=lambda s: int(s.split()[0])):
    print(r)
if write:
    open(sys.argv[2], "w").write("\n".join(out))
    print("written")
