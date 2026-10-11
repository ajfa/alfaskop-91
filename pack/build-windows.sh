#!/bin/bash
# build-windows.sh - build the Windows pack: out/Alfaskop91-DU4110-win/ and out/Alfaskop91-DU4110-windows.zip
# (from WSL, Linux or Git Bash; it only copies files and needs python3)
#
#   A91_EXE=/path/to/a91.exe ./build-windows.sh
#
# A91_EXE       a91.exe built as in build-a91-windows.sh (required)
# OUT           where the pack goes (default: pack/out)
# STRIP         a strip that knows PE files, to make a91.exe smaller (optional)
# MAINFRAME=0   leaves out the IBM mainframe mode. Otherwise it needs host3705/windows/build-windows.sh's output
#               (WORK_WINDOWS, default host3705/work/windows) and the TK5 that setup-tk5.sh prepared (TK5,
#               default host3705/work/mvs-tk5).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(dirname "$HERE")
A91_EXE=${A91_EXE:?set A91_EXE to a91.exe}
OUT=${OUT:-$HERE/out}
WORK_WINDOWS=${WORK_WINDOWS:-$REPO/host3705/work/windows}
TK5=${TK5:-$REPO/host3705/work/mvs-tk5}
PY=$(command -v python3 || command -v python)
W=$OUT/Alfaskop91-DU4110-win
P=$W/Alfaskop91-DU4110
F=$HERE/files

crlf() { sed 's/\r*$/\r/' "$1" > "$2"; }

rm -rf "$W"
mkdir -p "$P/disks/originals" "$P/snapshots" "$P/roms"
cp "$A91_EXE" "$P/a91.exe"
[ -n "$STRIP" ] && "$STRIP" "$P/a91.exe"
cp -r "$REPO/roms/a91" "$REPO/roms/alfaskop_s41_kb" "$P/roms/"
cp "$REPO"/media/mattis-lind/{A91R4A_1,A91R4A_2,A91SETUP}.IMD "$P/disks/originals/"
crlf "$F/ui.ini" "$P/ui.ini"
"$PY" "$HERE/readme.py" windows "$P/README.txt" > /dev/null
for b in START-A91 START-SETUP-DISK RESET-DISKS STOP; do crlf "$F/windows/$b.bat" "$P/$b.bat"; done

if [ "${MAINFRAME:-1}" != 0 ]; then
	M=$P/mainframe
	mkdir -p "$M/tk5"
	cp -r "$WORK_WINDOWS/i3705" "$WORK_WINDOWS/hercules" "$M/"
	# TK5 as setup-tk5.sh left it, without its own Hercules builds, documentation and logs
	tar -C "$TK5" --exclude=./hercules --exclude=./doc --exclude=./Packages --exclude=./mvs_osx \
		--exclude='./log/*' --exclude='./prt/*' --exclude='./pch/*' -cf - . | tar -C "$M/tk5" -xf -
	mkdir -p "$M/tk5/hercules" "$M/tk5/log" "$M/tk5/prt" "$M/tk5/pch"
	cp -r "$TK5/hercules/httproot" "$M/tk5/hercules/"
	bash "$REPO/host3705/windows/tk5-localhost.sh" "$M/tk5" > /dev/null
	crlf "$F/windows/START-A91-MAINFRAME.bat" "$P/START-A91-MAINFRAME.bat"
	crlf "$F/windows/start-mainframe.ps1" "$M/start-mainframe.ps1"
fi

rm -f "$OUT/Alfaskop91-DU4110-windows.zip"
"$PY" "$HERE/mkzip.py" --windows "$P" "$OUT/Alfaskop91-DU4110-windows.zip"
