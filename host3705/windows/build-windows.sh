#!/bin/bash
# build-windows.sh <mvs-tk5 dir> [work dir] - in Git Bash, with MSYS2 in C:\msys64 (packages mingw-w64-x86_64-gcc
# and gcc): build the 3705 module for the Windows Hercules that comes with TK5, and the 3705 emulator, and lay
# them out in <work dir>/windows (default: work/ next to these scripts):
#   hercules\        TK5's hercules\windows\64 with the new hdt3705.dll
#   i3705\usr\bin\   i3705.exe and msys-2.0.dll, with i3705\dev\shm and i3705\tmp for the MSYS2 runtime
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
TK5=$(cd "${1:?usage: build-windows.sh <mvs-tk5 dir> [work dir]}" && pwd)
OUT=${2:-$HERE/../work}/windows
MSYS=${MSYS:-/c/msys64}
HW=$TK5/hercules/windows/64
[ -f "$HW/hengine.dll" ] || { echo "no Hercules for Windows in $HW" >&2; exit 1; }
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
S=$OUT/src
mkdir -p "$S"

# the 3705 emulator, patched as on Linux
if [ ! -d "$S/IBM3705_R5" ]; then
	git clone -q https://github.com/snhstq/IBM3705_R5.git "$S/IBM3705_R5"
	git -c advice.detachedHead=false -C "$S/IBM3705_R5" checkout -q 73994e8
	I=$S/IBM3705_R5
	python3 "$HERE/../patches/i3705-line-address.py" "$I/I3705/i3705_lib.c" "$I/I3705/i3705_chan_T2.c"
	python3 "$HERE/../patches/channel-races.py" "$I/I3705/i3705_chan_T2.c" "$I/I3705/i3705_lib.c" "$I/Hercules files/comm3705.c"
	python3 "$HERE/../patches/read-length.py" "$I/I3705/i3705_chan_T2.c" "$I/Hercules files/comm3705.c"
fi
I=$S/IBM3705_R5

# the headers of Hercules 4.9.1, which TK5's Windows build comes from (4.9.1.11612-SDL-gee86c4de)
if [ ! -d "$S/hyperion" ]; then
	git -c advice.detachedHead=false clone -q --depth 1 --branch Release_4.9.1 https://github.com/SDL-Hercules-390/hyperion.git "$S/hyperion"
fi
H=$S/h3705
rm -rf "$H"; mkdir -p "$H"
cp "$S"/hyperion/*.h "$S/hyperion/telnet/include/telnet.h" "$HERE/mingw_shim.h" "$HERE/shim/hires.c" "$H/"
cp "$I/Hercules files/comm3705.c" "$H/"
# GCC ignores __declspec(align(n)): without this DEVBLK and SYSBLK come out smaller and Hercules refuses the module
sed -i 's|#define __ALIGN(_n)     __declspec(align(_n))|#define __ALIGN(_n)     __attribute__((aligned(_n)))|' "$H/hmalloc.h"
# MinGW already has pid_t and mode_t
sed -i 's/^  typedef    int32_t            pid_t;/  #ifndef __MINGW32__\n  typedef    int32_t            pid_t;\n  #endif/' "$H/htypes.h"
sed -i 's/^  typedef    int32_t            mode_t;/  #ifndef __MINGW32__\n  typedef    int32_t            mode_t;\n  #endif/' "$H/htypes.h"
grep -q "aligned(_n)" "$H/hmalloc.h" && grep -q "__MINGW32__" "$H/htypes.h"

cd "$H"
(
	export PATH=$MSYS/mingw64/bin:$PATH
	gcc -c -O2 -include mingw_shim.h -D_MSVC_ -DMAX_CPU_ENGS=64 -DHOST_ARCH=AMD64 -D_WIN64 -D_CRT_SECURE_NO_DEPRECATE \
		-D_CRT_NONSTDC_NO_DEPRECATE -DFD_SETSIZE=1024 -mcx16 -w -Wno-implicit-function-declaration \
		-Wno-incompatible-pointer-types -Wno-int-conversion -I. comm3705.c -o comm3705.o
	gcc -c -O2 hires.c -o hires.o
	# the same C runtime as TK5's Hercules (MSVCR90, Visual Studio 2008)
	gcc -shared -o hdt3705.dll comm3705.o hires.o "$HW/hengine.dll" "$HW/hutil.dll" "$HW/hsys.dll" \
		-lws2_32 -lwinmm -mcrtdll=msvcr90 -static-libgcc
	strip hdt3705.dll
)

cd "$I"
rm -rf shim && cp -r "$HERE/shim" shim
(
	export PATH=$MSYS/usr/bin:$PATH
	mkdir -p BIN
	gcc -std=gnu99 -U__STRICT_ANSI__ -O2 -fno-strict-overflow -w -Wno-incompatible-pointer-types -include dlfcn.h \
		-I shim -I . -D_GNU_SOURCE -DHAVE_SEMAPHORE \
		I3705/i3705_cpu.c I3705/i3705_chan_T2.c I3705/i3705_scan_T2.c I3705/i3705_sys.c I3705/i3705_lib.c \
		I3705/i3705_panel.c scp.c sim_console.c sim_fio.c sim_timer.c sim_sock.c sim_tmxr.c sim_ether.c sim_tape.c \
		sim_shmem.c shim/hires.c -I I3705 -o BIN/i3705.exe -lm -lwinmm -fcommon
)

rm -rf "$OUT/hercules" "$OUT/i3705"
cp -r "$HW" "$OUT/hercules"
cp "$H/hdt3705.dll" "$OUT/hercules/"
mkdir -p "$OUT/i3705/usr/bin" "$OUT/i3705/dev/shm" "$OUT/i3705/tmp"
cp "$I/BIN/i3705.exe" "$MSYS/usr/bin/msys-2.0.dll" "$OUT/i3705/usr/bin/"
cp "$I/3705-128k.cnf" "$OUT/i3705/"
ls -la "$OUT/hercules/hdt3705.dll" "$OUT/i3705/usr/bin/i3705.exe"
