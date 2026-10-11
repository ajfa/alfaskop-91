"""mkzip.py [--windows] <dir> <zip> : zip a pack.

Linux (default): Unix modes are kept in the zip (755 for .sh and the executables, 644 for the rest) and
text files get LF line ends. --windows: files go in as they are, CRLF included."""
import os, stat, sys, zipfile

args = sys.argv[1:]
windows = "--windows" in args
args = [a for a in args if a != "--windows"]
src, out = args
base = os.path.dirname(os.path.abspath(src))
exe = {"a91", "i3705", "hercules"}
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for root, dirs, files in os.walk(src):
        dirs.sort()
        rel_root = os.path.relpath(root, base)
        info = zipfile.ZipInfo(rel_root + "/")
        info.create_system = 0 if windows else 3
        info.external_attr = 0x10 if windows else (stat.S_IFDIR | 0o755) << 16
        z.writestr(info, b"")
        for f in sorted(files):
            p = os.path.join(root, f)
            data = open(p, "rb").read()
            info = zipfile.ZipInfo(os.path.join(rel_root, f))
            info.compress_type = zipfile.ZIP_DEFLATED
            if windows:
                info.create_system = 0
            else:
                if f.endswith((".sh", ".txt", ".ini", ".lua")):
                    data = data.replace(b"\r\n", b"\n")
                mode = 0o755 if (f.endswith(".sh") or f in exe) else 0o644
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | mode) << 16
            z.writestr(info, data)
print(out, os.path.getsize(out))
