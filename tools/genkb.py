"""Generate the alfaskop_s41_kb INPUT_PORTS from an Ericsson keyboard table.

  genkb.py <table.bin> <alfaskop_s41_kb.cpp> <keymap.lua>

Key numbers are the codes the KBC reports (column * 8 + row); port P1n bit b
is key number n * 16 + b.  Plane 0 of the table gives the unshifted meaning,
plane 1 the shifted one.  Codes 24, 32 and 108 never reach the host as keys:
the DUOS loads them into the KBC as Shift Lock, Shift and Alt.
"""
import re, sys

tab = open(sys.argv[1], "rb").read()

def ent(p, i):
    return (tab[p * 256 + i * 2] << 8) | tab[p * 256 + i * 2 + 1]

NAT = {0x28E5: (0xe5, 0xc5), 0x28E3: (0xe4, 0xc4), 0x28F8: (0xf6, 0xd6), 0x28E7: (0xfc, 0xdc)}

# key number -> (name, keycode or None, [chars]) for everything the table alone does not settle
OVR = {
    24: ("Shift Lock", "CAPSLOCK", ["UCHAR_MAMEKEY(CAPSLOCK)"]),
    32: ("Shift", "LSHIFT", ["UCHAR_SHIFT_1"]),
    108: ("Alt", "LALT", ["UCHAR_SHIFT_2"]),
    8: ("Space", "SPACE", ["' '"]),
    70: ("New Line", "ENTER_PAD", []),
    103: ("Enter", "ENTER", ["13"]),
    107: ("Enter (2)", None, []),
    38: ("Tab", "TAB", ["9"]),
    86: ("Back Tab", None, []),
    39: ("Cursor Left", "LEFT", ["UCHAR_MAMEKEY(LEFT)"]),
    90: ("Cursor Left (2)", "BACKSPACE", ["8"]),
    115: ("Cursor Right", "RIGHT", ["UCHAR_MAMEKEY(RIGHT)"]),
    114: ("Delete", "DEL", ["UCHAR_MAMEKEY(DEL)"]),
    95: ("Line Start", None, []),
    106: ("Line Start (2)", None, []),
    112: ("Erase Input", None, []),
    117: ("Clear", "END", []),
    121: ("Upper Case", None, []),
    99: ("Print / Alt: Assign Printer", "PRTSCR", []),
    100: ("Key 100 (4221) / Alt: Dev Cancel", "TILDE", []),
    87: ("Cursor Down", "DOWN", ["UCHAR_MAMEKEY(DOWN)"]),
    23: ("Cursor Up", "UP", ["UCHAR_MAMEKEY(UP)"]),
    55: ("Key 55 (0206)", None, []),
    17: ("Home", "HOME", ["UCHAR_MAMEKEY(HOME)"]),
    98: ("Home (2)", None, []),
    91: ("Cursor New Line", None, []),
    7: ("Insert", "INSERT", ["UCHAR_MAMEKEY(INSERT)"]),
    54: ("Field Mark", None, []),
    6: ("PA1", "PGUP", []),
    14: ("PA1 (2)", None, []),
    22: ("PA2", "PGDN", []),
    109: ("PA2 (2)", None, []),
    1: ("PA3", None, []),
    104: ("å Å", "OPENBRACE", ["0x00e5", "0x00c5"]),
    89: ("é É", "EQUALS", ["0x00e9", "0x00c9"]),
    69: ("ö Ö", "COLON", ["0x00f6", "0x00d6"]),
    93: ("ä Ä", "QUOTE", ["0x00e4", "0x00c4"]),
    105: ("ü ^", "CLOSEBRACE", ["0x00fc", "'^'"]),
    12: ("3 #", "3", ["'3'", "'#'"]),
    13: ("4 ¤", "4", ["'4'", "0x00a4"]),
    92: ("' *", "BACKSLASH", ["'\\''", "'*'"]),
    88: ("+ ?", "MINUS", ["'+'", "'?'"]),
    33: ("< >", "BACKSLASH2", ["'<'", "'>'"]),
    75: (", ;", "COMMA", ["','", "';'"]),
    76: (". :", "STOP", ["'.'", "':'"]),
    77: ("- _", "SLASH", ["'-'", "'_'"]),
    16: ("KP .", "DEL_PAD", []),
    111: ("KP -", "MINUS_PAD", []),
    119: ("KP *", "ASTERISK", []),
    123: ("KP +", "PLUS_PAD", []),
    0: ("KP 6", "6_PAD", []), 97: ("KP 3", "3_PAD", []), 101: ("KP 4", "4_PAD", []),
    124: ("KP 0", "0_PAD", []), 127: ("KP 5", "5_PAD", []),
    83: ("KP 7", "7_PAD", []), 84: ("KP 8", "8_PAD", []), 85: ("KP 9", "9_PAD", []),
}
PF = {}
for i in range(9):
    PF[0x43F1 + i] = i + 1
    PF[0x43C1 + i] = i + 13
for i in range(3):
    PF[0x437A + i] = i + 10
    PF[0x434A + i] = i + 22

def describe(k):
    if k in OVR:
        return OVR[k]
    w0, w1 = ent(0, k), ent(1, k)
    kind, c0, c1 = w0 >> 8, w0 & 0xff, w1 & 0xff
    if kind == 0x28 and 0x61 <= c0 <= 0x7a:
        return (chr(c0).upper(), chr(c0).upper(), ["'%s'" % chr(c0), "'%s'" % chr(c0).upper()])
    if kind == 0x30 and 0x30 <= c0 <= 0x39 and w1 != w0:
        if not (0x20 < c1 < 0x7f):
            return (chr(c0), chr(c0), ["'%s'" % chr(c0)])
        sh = chr(c1)
        return ("%s %s" % (chr(c0), sh.replace('"', '\\"')), chr(c0), ["'%s'" % chr(c0), "'%s'" % sh.replace("'", "\\'")])
    if w0 in PF:
        n = PF[w0]
        return ("PF%d" % n if n <= 12 else "PF%d (Shift+F%d)" % (n, n - 12), "F%d" % n if n <= 12 else None, ["UCHAR_MAMEKEY(F%d)" % n] if n <= 12 else [])
    if w0 == 0x4223 and w1 == 0x4223:
        return (None, None, [])
    return ("Key %d (%04X)" % (k, w0), None, [])

lines = ["INPUT_PORTS_START(alfaskop_s41_kb)"]
lua = ["keymap = {"]
used = set()
for p in range(8):
    lines.append('\tPORT_START("P1%d")' % p)
    for b in range(16):
        k = p * 16 + b
        name, kc, chars = describe(k)
        if name is None:
            lines.append("\tPORT_BIT( 0x%04x, IP_ACTIVE_LOW, IPT_UNUSED ) // %d" % (1 << b, k))
            continue
        assert kc is None or kc not in used, (k, kc)
        if kc:
            used.add(kc)
        s = '\tPORT_BIT( 0x%04x, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("%s")' % (1 << b, name)
        if kc:
            s += " PORT_CODE(KEYCODE_%s)" % kc
        for c in chars:
            s += " PORT_CHAR(%s)" % c
        lines.append(s + " // %d" % k)
        lua.append('  [%d] = { "P1%d", 0x%04x, "%s" },' % (k, p, 1 << b, name.replace('\\"', '"').replace('"', '\\"')))
lines.append("INPUT_PORTS_END")
lua.append("}")

src = open(sys.argv[2], encoding="utf-8").read()
src = re.sub(r"INPUT_PORTS_START\(alfaskop_s41_kb\).*?INPUT_PORTS_END", lambda m: "\n".join(lines), src, flags=re.S)
open(sys.argv[2], "w", encoding="utf-8").write(src)
open(sys.argv[3], "w", encoding="utf-8").write("\n".join(lua) + "\n")
print("\n".join(lines))
