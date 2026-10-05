#!/bin/bash
# common.sh - locations and helpers shared by the host3705 scripts; sourced, not run.
# Every location can be overridden from the environment.

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
WORK=${WORK:-$HERE/work}
TK5=${TK5:-$WORK/mvs-tk5}                  # unpacked mvs-tk5.zip
I3705=${I3705:-$WORK/IBM3705_R5}           # IBM 3705 emulator checkout, i3705 built in BIN/
HERC=${HERC:-$WORK/hercules}               # Hercules 4.6 install prefix, built with the 3705 comm3705.c
RUN=${RUN:-$WORK/run}                      # logs and the console pipe
A91_MAME=${A91_MAME:-a91}                  # MAME binary with the a91du machine
A91_ROMPATH=${A91_ROMPATH:-roms}
A91_DISKS=${A91_DISKS:-.}                  # directory with A91R4A_1.IMD and A91R4A_2.IMD
LOG=$RUN/herc.log
mkdir -p "$RUN"

# pids of what these scripts start: the 3705 and Hercules by their binaries, the helpers that keep their
# input open by their working directory (never anything else of the same name)
our_pids() {
	local want=$1 d pid exe cwd
	local i3705_bin hercules_bin i3705_dir run_dir
	i3705_bin=$(readlink -f "$I3705/BIN/i3705"); hercules_bin=$(readlink -f "$HERC/bin/hercules")
	i3705_dir=$(readlink -f "$I3705"); run_dir=$(readlink -f "$RUN")
	for d in /proc/[0-9]*; do
		pid=${d#/proc/}
		[ "$pid" = $$ ] && continue
		exe=$(readlink "$d/exe" 2>/dev/null); cwd=$(readlink "$d/cwd" 2>/dev/null)
		case "$want:$exe:$cwd" in
			3705:"$i3705_bin":*|3705:*/tail:"$i3705_dir") echo "$pid" ;;
			mvs:"$hercules_bin":*|mvs:*/sleep:"$run_dir") echo "$pid" ;;
		esac
	done
}

start_3705() {
	stop_3705
	(cd "$I3705" && I3705_LINE_ADDR=127.0.0.1 nohup bash -c "tail -f /dev/null | stdbuf -oL -eL ./BIN/i3705 3705-128k.cnf" \
		> "$RUN/i3705.log" 2>&1 &)
	for _ in $(seq 1 20); do grep -q "Line-0 ready" "$RUN/i3705.log" 2>/dev/null && return 0; sleep 1; done
	echo "i3705 did not start, see $RUN/i3705.log" >&2; return 1
}

# the address the 3705 listens on for its lines (127.0.0.1, from I3705_LINE_ADDR)
line_address() {
	grep -o "Using TCP network Address [0-9.]*" "$RUN/i3705.log" | awk '{print $NF}'
}

start_mvs() {
	[ -p "$RUN/herc.in" ] || mkfifo "$RUN/herc.in"
	(cd "$RUN" && nohup sleep 2147483647 > "$RUN/herc.in" 2>/dev/null &)
	: > "$LOG"
	(cd "$TK5" && LD_LIBRARY_PATH=$HERC/lib:$HERC/lib/hercules HERCULES_RC=scripts/ipl.rc \
		nohup "$HERC/bin/hercules" -d -f conf/tk5.cnf < "$RUN/herc.in" > "$LOG" 2>&1 &)
}

stop_mvs() {
	local hercules_bin
	hercules_bin=$(readlink -f "$HERC/bin/hercules")
	if our_pids mvs | xargs -r -I{} readlink /proc/{}/exe | grep -qxF "$hercules_bin"; then
		echo quit > "$RUN/herc.in"
		for _ in $(seq 1 30); do
			our_pids mvs | xargs -r -I{} readlink /proc/{}/exe | grep -qxF "$hercules_bin" || break
			sleep 2
		done
	fi
	our_pids mvs | xargs -r kill 2>/dev/null || true
	return 0
}

stop_3705() {
	local i3705_bin p
	i3705_bin=$(readlink -f "$I3705/BIN/i3705")
	# the 3705 first, then what keeps its input open
	for p in $(our_pids 3705); do [ "$(readlink /proc/$p/exe 2>/dev/null)" = "$i3705_bin" ] && kill "$p" 2>/dev/null; done
	sleep 1
	our_pids 3705 | xargs -r kill 2>/dev/null || true
}

# MVS console command, e.g. mvs "v net,act,id=N16A"; a Hercules command without the slash: herc "devlist"
mvs() { echo "/$*" > "$RUN/herc.in"; }
herc() { echo "$*" > "$RUN/herc.in"; }

mark() { if [ -f "$LOG" ]; then wc -l < "$LOG"; else echo 0; fi; }

# wait_for <log line mark> <seconds> <grep -E pattern>: true when the pattern shows up after the mark
wait_for() {
	local from=$1 secs=$2 pat=$3 i
	for i in $(seq 1 "$secs"); do
		tail -n +"$((from + 1))" "$LOG" | grep -aqE "$pat" && return 0
		sleep 1
	done
	return 1
}

wait_ipl() {
	wait_for 0 600 "IKT005I" || { echo "MVS did not come up, see $LOG" >&2; return 1; }
	sleep 8
}

# submit <file.jcl> <jobname>: send the deck to the socket reader, wait until JES2 purges the job, print
# its step completion codes and fail if one is above MAXCC (default 4) or a step was not run or abended
submit() {
	local jcl=$1 job=$2 m prt=$TK5/prt/prt00e.txt p0 out
	m=$(mark); p0=$(wc -c < "$prt" 2>/dev/null || echo 0)
	cat "$jcl" > /dev/tcp/127.0.0.1/3505 || return 1
	wait_for "$m" 600 "$job +IS PURGED" || { echo "$job did not finish" >&2; return 1; }
	sleep 2
	out=$(tail -c +"$((p0 + 1))" "$prt" | tr -d '\r\f' | grep -aE "IEF(142|272|450|452|453)I $job " || true)
	echo "$out" | sed 's/^ *//' | cut -c1-100
	echo "$out" | grep -qE "IEF(272|450|452|453)I" && return 1
	echo "$out" | grep -o "COND CODE [0-9]*" | awk -v max="${MAXCC:-4}" '$3 + 0 > max {bad = 1} END {exit bad}'
}

# punch <file.jcl> <jobname> <out>: run a job whose SYSOUT=B goes to the card punch and keep what it punched
punch() {
	local pch=$TK5/pch/pch00d.txt p0
	p0=$(wc -c < "$pch" 2>/dev/null || echo 0)
	mvs '$s pun1'; sleep 2
	submit "$1" "$2" > /dev/null || return 1
	sleep 3
	tail -c +"$((p0 + 1))" "$pch" | tr -d '\r' > "$3"
	[ -s "$3" ]
}
