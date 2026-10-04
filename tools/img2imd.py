"""img2imd.py in.img out.IMD [mode]  - 80x2x9x512 flat image to ImageDisk, sectors 1..9 in order."""
import sys, time
src, dst = sys.argv[1], sys.argv[2]
mode = int(sys.argv[3]) if len(sys.argv) > 3 else 4
d = open(src, "rb").read()
assert len(d) == 80 * 2 * 9 * 512, len(d)
out = bytearray(time.strftime("IMD 1.18: %d/%m/%Y %H:%M:%S\r\n").encode())
out += b"derived from a flat image by img2imd.py\r\n\x1a"
i = 0
for c in range(80):
    for h in range(2):
        out += bytes([mode, c, h, 9, 2]) + bytes(range(1, 10))
        for s in range(9):
            sec = d[i:i + 512]; i += 512
            if sec == bytes([sec[0]]) * 512:
                out += bytes([2, sec[0]])
            else:
                out += b"\x01" + sec
open(dst, "wb").write(out)
print(dst, len(out))
