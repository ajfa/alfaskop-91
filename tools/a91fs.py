"""List and extract files from an Alfaskop 91 diskette (flat 80x2x9x512 image).

  a91fs.py <image> [outdir]

A directory entry is 32 bytes: type letter, 8-char name, then
  [14] 0x89 (5.25 inch), [16:18] cyl/head+sector of the sector BEFORE the data,
  [18:20] record count, [20:22] record size, [26:28] last record size
  (libraries: [26:28] is the next free position), [28:32] load address.
Libraries (type D) hold the same 32-byte entries for their members.
"""
import os, struct, sys

def lba(cs):
    c, hs = cs >> 8, cs & 0xff
    return (c * 2 + (hs >> 7)) * 9 + ((hs & 0x7f) - 1)

def entries(img, base, limit, stop=False):
    out = []
    for off in range(base, base + limit, 32):
        r = img[off:off + 32]
        if len(r) < 32 or r[14] != 0x89 or not (0x41 <= r[0] <= 0x5a):
            if stop and out:
                break
            continue
        name = r[1:9].decode("latin1").rstrip()
        start, nrec, rsz = struct.unpack(">HHH", r[16:22])
        last = struct.unpack(">H", r[26:28])[0]
        load = struct.unpack(">I", r[28:32])[0]
        out.append(dict(type=chr(r[0]), name=name, start=start, nrec=nrec, rsz=rsz,
                        last=last, load=load, raw=r))
    return out

def data_of(img, e):
    a = (lba(e["start"]) + 1) * 512
    if e["type"] == "D":
        b = (lba(e["last"]) + 1) * 512
        return img[a:b]
    n = (e["nrec"] - 1) * e["rsz"] + e["last"] if e["nrec"] else 0
    return img[a:a + n]

def main():
    img = open(sys.argv[1], "rb").read()
    outdir = sys.argv[2] if len(sys.argv) > 2 else None
    print("volume:", img[1:13].decode("latin1"), img[13:40].decode("latin1"))
    for e in entries(img, 0x200, 0x800):
        d = data_of(img, e)
        print("%s %-8s start=%04X nrec=%4d rsz=%4X last=%4X load=%08X  %6d bytes"
              % (e["type"], e["name"], e["start"], e["nrec"], e["rsz"], e["last"], e["load"], len(d)))
        if outdir:
            os.makedirs(outdir, exist_ok=True)
            open(os.path.join(outdir, "%s_%s.bin" % (e["type"], e["name"])), "wb").write(d)
        if e["type"] == "D":
            for m in entries(d, 0, min(len(d), 0x800), stop=True):
                md = data_of(img, m)
                print("    %s %-8s start=%04X nrec=%4d rsz=%4X last=%4X load=%08X  %6d bytes"
                      % (m["type"], m["name"], m["start"], m["nrec"], m["rsz"], m["last"], m["load"], len(md)))
                if outdir:
                    sub = os.path.join(outdir, e["name"])
                    os.makedirs(sub, exist_ok=True)
                    open(os.path.join(sub, "%s_%s_%08X.bin" % (m["type"], m["name"], m["load"])), "wb").write(md)

main()
