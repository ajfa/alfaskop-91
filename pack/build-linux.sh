#!/bin/bash
# build-linux.sh - build the Linux pack: out/Alfaskop91-DU4110/ and out/Alfaskop91-DU4110-linux.zip
#
#   A91_BIN=/path/to/a91 ./build-linux.sh
#
# A91_BIN  the a91 built as in the top README (required)
# OUT      where the pack goes (default: pack/out)
# MAINFRAME=0 leaves out the IBM mainframe mode. Otherwise it needs what host3705/prepare.sh and
# host3705/setup-tk5.sh leave in host3705/work, or the same locations given as I3705, HERC and TK5.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(dirname "$HERE")
A91_BIN=${A91_BIN:?set A91_BIN to the a91 executable}
OUT=${OUT:-$HERE/out}
WORK=${WORK:-$REPO/host3705/work}
I3705=${I3705:-$WORK/IBM3705_R5}
HERC=${HERC:-$WORK/hercules}
TK5=${TK5:-$WORK/mvs-tk5}
P=$OUT/Alfaskop91-DU4110
F=$HERE/files

rm -rf "$P"
mkdir -p "$P/lib" "$P/disks/originals" "$P/snapshots"
cp "$A91_BIN" "$P/a91"
strip "$P/a91"
for l in libSDL2-2.0.so.0 libSDL2_ttf-2.0.so.0; do
	cp -L "$(ldconfig -p | awk -v l=$l '$1 == l { print $NF; exit }')" "$P/lib/"
done
mkdir -p "$P/roms"
cp -r "$REPO/roms/a91" "$REPO/roms/alfaskop_s41_kb" "$P/roms/"
cp "$REPO"/media/mattis-lind/{A91R4A_1,A91R4A_2,A91SETUP}.IMD "$P/disks/originals/"
chmod 444 "$P"/disks/originals/*.IMD
cp "$F/ui.ini" "$P/"
python3 "$HERE/readme.py" linux "$P/README.txt" > /dev/null

common='cd "$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"'
cat > "$P/START-A91.sh" <<S
#!/bin/sh
# Alfaskop 91 + DU 4110 with the R4A diskettes (system and 3270 emulation) and the emulated host
$common
for d in A91R4A_1 A91R4A_2; do [ -f disks/\$d.IMD ] || cp disks/originals/\$d.IMD disks/; chmod u+w disks/\$d.IMD; done
exec ./a91 a91du -rompath roms -flop1 disks/A91R4A_1.IMD -flop2 disks/A91R4A_2.IMD -window -nomaximize -skip_gameinfo -snapshot_directory snapshots -cfg_directory cfg "\$@"
S
cat > "$P/START-SETUP-DISK.sh" <<S
#!/bin/sh
# Alfaskop 91 + DU 4110 with the A91SETUP diskette (system M5.06-02); it only gets to the manual load line
$common
[ -f disks/A91SETUP.IMD ] || cp disks/originals/A91SETUP.IMD disks/
chmod u+w disks/A91SETUP.IMD
exec ./a91 a91du -rompath roms -flop1 disks/A91SETUP.IMD -window -nomaximize -skip_gameinfo -snapshot_directory snapshots -cfg_directory cfg "\$@"
S
cat > "$P/RESET-DISKS.sh" <<'S'
#!/bin/sh
# Close the emulator and put the working diskettes back as the originals
cd "$(dirname "$(readlink -f "$0")")"
./STOP.sh > /dev/null
for d in A91R4A_1 A91R4A_2 A91SETUP; do cp -f disks/originals/$d.IMD disks/; chmod u+w disks/$d.IMD; done
echo "Working diskettes restored from disks/originals."
S
cat > "$P/STOP.sh" <<'S'
#!/bin/bash
# Close the emulator, Hercules and the 3705 of this pack if they were left running (only this pack's)
cd "$(dirname "$(readlink -f "$0")")"
P=$PWD
for d in /proc/[0-9]*; do
	pid=${d#/proc/}
	[ "$pid" = $$ ] && continue
	exe=$(readlink "$d/exe" 2>/dev/null); cwd=$(readlink "$d/cwd" 2>/dev/null)
	case "$exe:$cwd" in
		"$P/a91:"*|*:"$P/mainframe"|*:"$P/mainframe"/*) kill "$pid" 2>/dev/null ;;
	esac
done
echo "Emulator closed."
S

if [ "${MAINFRAME:-1}" != 0 ]; then
	M=$P/mainframe
	mkdir -p "$M/i3705" "$M/hercules/bin" "$M/hercules/lib/hercules" "$M/tk5"
	cp "$I3705/BIN/i3705" "$I3705/3705-128k.cnf" "$M/i3705/"
	cp "$HERC/bin/hercules" "$M/hercules/bin/"
	for f in "$HERC"/lib/lib*.so; do cp -L "$f" "$M/hercules/lib/"; done
	cp "$HERC"/lib/hercules/*.so "$M/hercules/lib/hercules/"
	strip --strip-unneeded "$M/i3705/i3705" "$M/hercules/bin/hercules" "$M"/hercules/lib/*.so "$M"/hercules/lib/hercules/*.so
	# TK5 as setup-tk5.sh left it, without its own Hercules builds, documentation and logs
	tar -C "$TK5" --exclude=./hercules --exclude=./doc --exclude=./Packages --exclude=./mvs_osx \
		--exclude='./log/*' --exclude='./prt/*' --exclude='./pch/*' -cf - . | tar -C "$M/tk5" -xf -
	mkdir -p "$M/tk5/hercules" "$M/tk5/log" "$M/tk5/prt" "$M/tk5/pch"
	cp -r "$TK5/hercules/httproot" "$M/tk5/hercules/"
	bash "$REPO/host3705/windows/tk5-localhost.sh" "$M/tk5" > /dev/null
	cp "$F/START-A91-MAINFRAME.sh" "$P/"
fi

chmod 755 "$P"/*.sh "$P/a91"
[ -d "$P/mainframe" ] && chmod 755 "$P/mainframe/i3705/i3705" "$P/mainframe/hercules/bin/hercules"
python3 "$HERE/mkzip.py" "$P" "$OUT/Alfaskop91-DU4110-linux.zip"
