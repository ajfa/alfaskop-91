#!/bin/bash
# setup-tk5.sh - one-time changes to a fresh TK5 so it can load a real NCP into the emulated 3705 and
# let the Alfaskop 91 log on to TSO. Runs MVS without a window; takes about 15 minutes.
set -e
. "$(dirname "$0")/common.sh"
J=$RUN/jcl
mkdir -p "$J"

jobcard() { printf "//%-8s JOB (1),'%s',CLASS=A,MSGCLASS=A,MSGLEVEL=(1,1),\n//         USER=HERC01,PASSWORD=CUL8TR%s\n" "$1" "$2" "${3:+,$3}"; }
# pchmem <job> <library> <member> <out>: punch one member to a file
pchmem() {
	{ jobcard "$1" "PUNCH $3"; printf '//PUNCH    EXEC PGM=IEBGENER\n//SYSPRINT DD SYSOUT=A\n//SYSIN    DD DUMMY\n'
	  printf '//SYSUT1   DD DSN=%s(%s),DISP=SHR\n//SYSUT2   DD SYSOUT=B\n//\n' "$2" "$3"; } > "$J/$1.jcl"
	punch "$J/$1.jcl" "$1" "$4"
	echo "punched $2($3): $(grep -c '' "$4") cards"
}
step() { echo; echo "== $*"; }

step "starting the 3705 and MVS"
stop_mvs; start_3705; start_mvs; wait_ipl

step "1/4 punching LNKLST00, VATLST00 and ATCCON01"
pchmem PCHLNK SYS1.PARMLIB LNKLST00 "$J/LNKLST00"
pchmem PCHVAT SYS1.PARMLIB VATLST00 "$J/VATLST00"
pchmem PCHCON SYS1.VTAMLST ATCCON01 "$J/ATCCON01"

step "2/4 catalog the NCP volume, link list, volume list, VTAM start list, fake IFLOADRN out, N16A in"
python3 "$HERE/tk5jobs.py" setup "$J/LNKLST00" "$J/VATLST00" "$J/ATCCON01" > "$J/setup.jcl"
submit "$J/setup.jcl" SETUP

step "re-IPL for the new link list"
# the 3705 does not take a second channel connection: it starts afresh with every IPL
stop_mvs; start_3705; start_mvs; wait_ipl

step "3/4 NCP generation, stage 1 (punches the stage 2 deck) and stage 2"
{ jobcard NCPGEN1 "NCP STAGE 1" REGION=4096K; cat "$HERE/jcl/ncpgen1.jcl"; } > "$J/ncpgen1.jcl"
punch "$J/ncpgen1.jcl" NCPGEN1 "$J/stage2.txt"
python3 "$HERE/tk5jobs.py" stage2 "$J/stage2.txt" > "$J/ncpgen2.jcl"
job2=$(head -1 "$J/ncpgen2.jcl" | cut -c3- | awk '{print $1}')
submit "$J/ncpgen2.jcl" "$job2"

step "4/4 N16A for the Alfaskop 91"
pchmem PCHN16A SYS1.VTAMLST N16A "$J/N16A"
python3 "$HERE/tk5jobs.py" n16a "$J/N16A" > "$J/updn16a.jcl"
submit "$J/updn16a.jcl" UPDN16A

step "loading the NCP once"
m=$(mark); mvs "v net,act,id=N16A"
ok=0; wait_for "$m" 300 "IST093I  N16A +ACTIVE" || ok=$?
stop_mvs; stop_3705
[ $ok = 0 ] || { echo "N16A did not load, see $LOG"; exit 1; }
echo "N16A loaded and active"
echo; echo "TK5 is ready: run.sh starts everything"
