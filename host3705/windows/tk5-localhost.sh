#!/bin/bash
# tk5-localhost.sh <tk5 dir> - make TK5 listen on 127.0.0.1 only: 3270 console port, card reader and the
# 2703 lines; the CTCT link and the Hercules web server are not started (Windows would otherwise ask about its firewall)
set -e
C=$1/conf
sed -i 's/^CNSLPORT ${CNSLPORT:=3270}/CNSLPORT ${CNSLPORT:=127.0.0.1:3270}/' "$C/tk5.cnf"
sed -i 's/${RDRPORT:=3505} sockdev/${RDRPORT:=127.0.0.1:3505} sockdev/' "$C/tk5.cnf"
sed -i 's/^HTTP /#HTTP /' "$C/tk5.cnf"
sed -i -E 's/^([0-9A-F]{4} 2703 dial=in )lport=/\1lhost=127.0.0.1 lport=/' "$C/tk5_default.cnf"
# the two CTCT devices (an MVS-to-MVS link nobody uses here) listen on every interface and take no address
sed -i -E 's/^(061[01] CTCT )/#\1/' "$C/tk5_default.cnf"
grep -n "^CNSLPORT\|sockdev\|HTTP" "$C/tk5.cnf"
grep -c "lhost=127.0.0.1" "$C/tk5_default.cnf"
# the debug trace of comm3705 was a timing crutch for the races patches/ fixes; without it the log stays small
sed -i 's/^\(0660 3705 adaptip=127.0.0.1 port=37051\) debug=yes/\1/' "$C/tk5_default.cnf"
grep -n "^0660" "$C/tk5_default.cnf"
