#!/bin/bash
# Alfaskop 91 + DU 4110 on an emulated IBM mainframe: the 3705 with its NCP, and MVS 3.8j (TK5) with TSO.
# The 3705 and Hercules have no window; the A91 has its own. Extra arguments go to a91 as they are.
cd "$(dirname "$(readlink -f "$0")")"
P=$PWD
M=$P/mainframe
L=$M/log
HLOG=$L/hercules.log
mkdir -p "$L"
export LD_LIBRARY_PATH="$P/lib:$M/hercules/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# only this pack's processes: a91 by its path, the rest by their working directory under mainframe/
stop_pack() {
	local d pid exe cwd
	for d in /proc/[0-9]*; do
		pid=${d#/proc/}
		[ "$pid" = $$ ] && continue
		exe=$(readlink "$d/exe" 2>/dev/null); cwd=$(readlink "$d/cwd" 2>/dev/null)
		case "$exe:$cwd" in
			"$P/a91:"*|*:"$M"|*:"$M"/*) kill "$pid" 2>/dev/null ;;
		esac
	done
}

lines() { [ -f "$HLOG" ] && wc -l < "$HLOG" || echo 0; }

# wait_for <from line> <seconds> <pattern>
wait_for() {
	local i
	for i in $(seq 1 "$2"); do
		tail -n +"$(($1 + 1))" "$HLOG" 2>/dev/null | grep -aqE "$3" && return 0
		[ $((i % 10)) = 0 ] && printf .
		sleep 1
	done
	return 1
}

mvs() { echo "/$*" > "$L/herc.in"; }

finish() {
	if pgrep -f "^$M/hercules/bin/hercules" > /dev/null 2>&1; then
		echo quit > "$L/herc.in" 2>/dev/null
		for _ in $(seq 1 30); do pgrep -f "^$M/hercules/bin/hercules" > /dev/null 2>&1 || break; sleep 2; done
	fi
	stop_pack
}

error() { echo; echo "ERROR: $*"; finish; exit 1; }

stop_pack
sleep 1
echo "Alfaskop 91 on an IBM mainframe: 3705 communications controller and MVS 3.8j (TK5)."
echo
echo "Starting the 3705 and MVS, about 2 minutes. Nothing needs to be typed in this window."
rm -f "$HLOG" "$L/herc.in"
mkfifo "$L/herc.in"
(cd "$M/i3705" && I3705_LINE_ADDR=127.0.0.1 nohup bash -c "tail -f /dev/null | stdbuf -oL -eL ./i3705 3705-128k.cnf" > "$L/i3705.log" 2>&1 &)
sleep 2
(cd "$L" && nohup sleep 2147483647 > "$L/herc.in" 2>/dev/null &)
(cd "$M/tk5" && HERCULES_RC=scripts/ipl.rc nohup "$M/hercules/bin/hercules" -d -p "$M/hercules/lib/hercules" -f conf/tk5.cnf < "$L/herc.in" > "$HLOG" 2>&1 &)
trap 'finish; exit 1' INT TERM
wait_for 0 600 "IKT005I" || error "MVS did not finish starting; see $HLOG"
sleep 8
echo
echo "MVS is up. Loading the NCP into the 3705, about 40 seconds."
n=$(lines); mvs "v net,act,id=N16A"
wait_for "$n" 300 "IST093I  N16A +ACTIVE" || error "the NCP did not load; see $HLOG"
echo
echo "NCP loaded. Opening the Alfaskop 91."

for d in A91R4A_1 A91R4A_2; do [ -f disks/$d.IMD ] || cp disks/originals/$d.IMD disks/; chmod u+w disks/$d.IMD; done
./a91 a91du -rompath roms -flop1 disks/A91R4A_1.IMD -flop2 disks/A91R4A_2.IMD -bitb socket.127.0.0.1:37520 \
	-window -nomaximize -skip_gameinfo -snapshot_directory snapshots -cfg_directory cfg "$@" &
a91=$!

# the network calls the A91 at about 52 seconds; then the line, the PU and the LU are activated
sleep 57
for id in L16A20 P16A20A T16A20A1; do
	n=$(lines); mvs "v net,act,id=$id"
	if wait_for "$n" 20 "IST093I  $id +ACTIVE"; then echo "$id active"; else echo "$id did not become active; see $HLOG"; fi
done
echo
echo "Ready. In the A91 window, with the VTAM screen and SYS OP on the status line:"
echo "  LOGON APPLID(TSO) LOGMODE(MHP3278E)  and Enter"
echo "  if X 443 shows on the status line: left Ctrl (RESET) and F3"
echo "  HERC01, Enter; CUL8TR, Enter; Enter at the first ***; Page Up (PA1) at the ***"
echo "  of the fortune cookie; at READY, any TSO command (LISTCAT, LOGOFF)"
echo
echo "Closing the A91 window shuts down MVS and the 3705."
wait $a91
finish
