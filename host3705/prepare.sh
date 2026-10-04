#!/bin/bash
# prepare.sh <mvs-tk5.zip> - build the IBM 3705 emulator and Hercules 4.6 with its comm3705.c, unpack TK5
# and give it the NCP volume and the channel-attached 3705 at 660. Everything goes under $WORK.
set -e
. "$(dirname "$0")/common.sh"
ZIP=${1:?usage: prepare.sh /path/to/mvs-tk5.zip}
I3705_COMMIT=${I3705_COMMIT:-73994e8}
mkdir -p "$WORK"

if [ ! -x "$I3705/BIN/i3705" ]; then
	git clone -q https://github.com/snhstq/IBM3705_R5.git "$I3705"
	git -c advice.detachedHead=false -C "$I3705" checkout -q "$I3705_COMMIT"
	make -C "$I3705" i3705 > "$WORK/build-i3705.log" 2>&1
fi
echo "i3705: $(ls "$I3705/BIN/i3705")"

if [ ! -x "$HERC/bin/hercules" ]; then
	rm -rf "$WORK/hyperion"
	git -c advice.detachedHead=false clone -q --depth 1 --branch Release_4.6 https://github.com/SDL-Hercules-390/hyperion.git "$WORK/hyperion"
	cp "$I3705/Hercules files/comm3705.c" "$WORK/hyperion/comm3705.c"
	(cd "$WORK/hyperion" && ./autogen.sh && ./configure --prefix="$HERC" && make -j"$(nproc)" && make install) \
		> "$WORK/build-hercules.log" 2>&1
fi
echo "hercules: $(ls "$HERC/bin/hercules")"

if [ ! -d "$TK5" ]; then
	(cd "$(dirname "$TK5")" && unzip -q "$ZIP")
	[ -d "$TK5" ] || mv "$(dirname "$TK5")/mvs-tk5" "$TK5"
fi
cp -n "$I3705/Hercules files/ncpssp.3350" "$TK5/dasd/"
cfg=$TK5/conf/tk5_default.cnf
if ! grep -q "^0660 3705 adaptip" "$cfg"; then
	cp "$cfg" "$cfg.orig"
	# the fake 3705s at 660-66B go and the emulated one takes 660; without debug=yes the NCP load hung at times
	sed -i -E 's/^(06[6][0-9A-B] 3705 )/#\1/' "$cfg"
	sed -i '0,/^#0660 3705/s//0660 3705 adaptip=127.0.0.1 port=37051 debug=yes\n#0660 3705/' "$cfg"
fi
grep -q "ncpssp.3350" "$TK5/conf/local.cnf" || printf '#\n# IBM3705_R5 NCP volume\n#\n0244 3350 dasd/ncpssp.3350\n' >> "$TK5/conf/local.cnf"
grep -n "^0660\|^0244" "$cfg" "$TK5/conf/local.cnf"
echo "prepared: now run setup-tk5.sh"
