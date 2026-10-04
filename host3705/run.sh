#!/bin/bash
# run.sh [seconds] - start the 3705, IPL TK5, load NCP N16A and run the Alfaskop 91 in a window with its
# host line on 3705 line 020; the line, the PU and the LU are activated once the A91 has taken its call.
# With LUA=tso-logon.lua there is no window and the A91 logs on to TSO by itself.
. "$(dirname "$0")/common.sh"
SECS=${1:-0}
abs() { [ -e "$1" ] && realpath "$1" || command -v "$1"; }
MAMEBIN=$(abs "$A91_MAME") || { echo "no MAME binary: set A91_MAME" >&2; exit 1; }
ROMS=$(realpath "$A91_ROMPATH")
DISKS=$(realpath "$A91_DISKS")
[ -n "$LUA" ] && LUA=$(realpath "$LUA")
[ -f "$DISKS/A91R4A_1.IMD" ] && [ -f "$DISKS/A91R4A_2.IMD" ] || { echo "A91R4A_1.IMD and A91R4A_2.IMD not in $DISKS" >&2; exit 1; }

trap '[ -n "$KEEP" ] || { stop_mvs; stop_3705; }' EXIT
echo "starting the 3705 and MVS (about 2 minutes)"
stop_mvs; start_3705; start_mvs; wait_ipl || exit 1
m=$(mark); mvs "v net,act,id=N16A"
wait_for "$m" 300 "IST093I  N16A +ACTIVE" || { echo "N16A did not load, see $LOG" >&2; exit 1; }
echo "NCP N16A loaded"

# work on copies of the diskettes; snapshots and the logon log land here too
A=$RUN/a91; rm -rf "$A"; mkdir -p "$A"; cd "$A"
cp "$DISKS/A91R4A_1.IMD" "$DISKS/A91R4A_2.IMD" .; chmod u+w ./*.IMD
args=(a91du -rompath "$ROMS" -flop1 A91R4A_1.IMD -flop2 A91R4A_2.IMD -bitb "socket.$(line_address):37520"
	-skip_gameinfo -snapshot_directory "$A")
[ "$SECS" -gt 0 ] && args+=(-seconds_to_run "$SECS")
if [ -n "$LUA" ]; then
	env -u DISPLAY -u WAYLAND_DISPLAY -u XDG_SESSION_TYPE SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
		"$MAMEBIN" "${args[@]}" -video none -sound none -autoboot_script "$LUA" > mame.out 2>&1 &
else
	"$MAMEBIN" "${args[@]}" -window -nomaximize > mame.out 2>&1 &
fi
pid=$!
echo "Alfaskop 91 started; the network calls it at about 52 seconds"

# the NCP has no use for the line until the A91 has answered the call and its first XID
sleep "${ACT_DELAY:-57}"
for id in L16A20 P16A20A T16A20A1; do
	m=$(mark); mvs "v net,act,id=$id"
	if wait_for "$m" 20 "IST093I  $id +ACTIVE"; then echo "$id active"; else echo "$id not active, see $LOG"; fi
done
wait $pid
