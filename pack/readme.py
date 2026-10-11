"""readme.py linux|windows <out> : write the pack's README.txt from files/README.txt.in."""
import os, sys

target, out = sys.argv[1:3]
here = os.path.dirname(os.path.abspath(__file__))
text = open(os.path.join(here, "files", "README.txt.in"), encoding="utf-8").read()

if target == "linux":
    sub = {
        "@OS@": "Linux", "@EXT@": ".sh", "@SEP@": "/", "@EXE@": "a91", "@EXEPAD@": "    ",
        "@LIBS@": " with its SDL2 libraries",
        "@DESKTOP@": "; a normal Linux desktop, X11 or Wayland, is enough",
        "@STEP1@": "In a terminal, inside the pack's folder:  ./START-A91.sh",
        "@STEP1MF@": "In a terminal, inside the pack's folder:  ./START-A91-MAINFRAME.sh",
        "@REQ@": "",
        "@LIBLINE@": "  lib/                     SDL2 and SDL2_ttf, so as not to depend on the system's\n",
    }
    nl = "\n"
else:
    sub = {
        "@OS@": "Windows", "@EXT@": ".bat", "@SEP@": "\\", "@EXE@": "a91.exe", "@EXEPAD@": "",
        "@LIBS@": "", "@DESKTOP@": "",
        "@STEP1@": "Double-click START-A91.bat.",
        "@STEP1MF@": "Double-click START-A91-MAINFRAME.bat.",
        "@REQ@": "\nIt needs Microsoft's 64-bit Visual C++ 2008 library, which TK5's Hercules\n"
                 "uses and almost every Windows has. If it is missing, the text window says\n"
                 "so at the start.\n",
        "@LIBLINE@": "",
    }
    nl = "\r\n"

title = "ALFASKOP 91 + DU 4110 (Ericsson, 1988) - pack for " + sub["@OS@"]
sub["@ULINE@"] = "=" * len(title)
for k, v in sub.items():
    text = text.replace(k, v)
assert "@" not in text.replace("@users", ""), [l for l in text.splitlines() if "@" in l]
open(out, "w", encoding="utf-8", newline="").write(text.replace("\n", nl))
print("ok", out)
