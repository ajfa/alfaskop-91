#!/bin/bash
# build-a91-windows.sh <mame dir> - build a91.exe natively for Windows with MSYS2's MinGW64 compiler, from a
# MAME 0.288 tree with patches/mame0288-alfaskop91.patch applied (see the top README).
# Run it from an MSYS2 MINGW64 shell, with the packages mingw-w64-x86_64-gcc, mingw-w64-x86_64-python and make:
#   C:\msys64\usr\bin\env.exe MSYSTEM=MINGW64 /usr/bin/bash -l /path/to/build-a91-windows.sh /c/path/to/mame
set -e
cd "${1:?usage: build-a91-windows.sh <mame dir>}"
export PATH=/mingw64/bin:$PATH
make SUBTARGET=a91 SOURCES=src/mame/ericsson/a91.cpp,src/mame/ericsson/alfaskop41xx.cpp \
	NOWERROR=1 OPTIMIZE=1 SYMBOLS=0 PTR64=1 USE_QTDEBUG=0 -j"${JOBS:-4}"
ls -la a91.exe
