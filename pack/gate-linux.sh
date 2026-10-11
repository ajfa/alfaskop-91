#!/bin/bash
# gate-linux.sh <zip> [gate dir] - check the Linux pack as it is delivered: unzip it into a fresh folder with a
# space in its name, check modes and line ends, and run its launchers headless: START-A91.sh with tools/gate.lua
# (the host echoes what is typed) and, if the pack has it, START-A91-MAINFRAME.sh with host3705/tso-logon.lua.
# HIDE="dir1 dir2" renames those directories during the run, to show that the pack needs nothing outside it.
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(dirname "$HERE")
Z=$(readlink -f "${1:?usage: gate-linux.sh <zip> [gate dir]}")
G=${2:-$HERE/out/gate linux $(date +%H%M%S)}
mkdir -p "$G" && cd "$G" && unzip -q "$Z"
P="$G/Alfaskop91-DU4110"
cd "$P"
rc=0
for f in START-A91.sh START-SETUP-DISK.sh RESET-DISKS.sh STOP.sh a91; do
	[ -x "$f" ] || { echo "NOT EXECUTABLE: $f"; rc=1; }
done
grep -l $'\r' ./*.sh README.txt 2>/dev/null && { echo "CRLF FOUND"; rc=1; }
for d in $HIDE; do mv "$d" "$d.hidden"; done
trap 'for d in $HIDE; do mv "$d.hidden" "$d"; done' EXIT
H="env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_SESSION_TYPE SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy"

$H ./START-A91.sh -video none -sound none -seconds_to_run 110 -autoboot_script "$REPO/tools/gate.lua" > gate-normal.out 2>&1
echo "normal: $(head -1 gate-result.txt 2>/dev/null || echo 'no gate-result.txt')"
grep -q '^PASS' gate-result.txt 2>/dev/null || rc=1
grep -q 'Current Videodriver: dummy' gate-normal.out || echo "(no 'Current Videodriver: dummy' in gate-normal.out)"

if [ -x START-A91-MAINFRAME.sh ]; then
	t0=$(date +%s)
	$H ./START-A91-MAINFRAME.sh -video none -sound none -seconds_to_run 300 \
		-autoboot_script "$REPO/host3705/tso-logon.lua" > gate-mainframe.out 2>&1
	echo "mainframe: $(( $(date +%s) - t0 )) s, $(grep -a '^== .* -> \|TSO OK' tso.txt 2>/dev/null | tail -1)"
	grep -aq 'TSO OK' tso.txt 2>/dev/null || rc=1
	grep -a "ERROR" gate-mainframe.out && rc=1
fi
left=$(ps -eo pid,args | grep -F "$P" | grep -v grep)
[ -n "$left" ] && { echo "left running:"; echo "$left"; rc=1; }
cmp -s disks/originals/A91R4A_1.IMD "$REPO/media/mattis-lind/A91R4A_1.IMD" || { echo "originals changed"; rc=1; }
echo "gate dir: $G"
echo "gate rc=$rc"
exit $rc
