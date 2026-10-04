# Notices and third-party material

## MAME

The patches in `patches/` apply to MAME 0.288 and are licensed BSD-3-Clause, the
same as the files they modify.

`src/mame/ericsson/a91.cpp` (also in `driver/`) is `license:BSD-3-Clause`. Its
copyright holder is **Mattis Lind**, who wrote the first version of the driver;
this repository extends it.

`src/mame/ericsson/alfaskop41xx.cpp`, which provides the DU 4110 and is extended
by the same patch, is `license:BSD-3-Clause` and its copyright holder is
**Joakim Larsson Edström**, who wrote the original Alfaskop driver in MAME.

The device and frontend files touched by the fixes keep their own copyright
holders and license.

MAME itself is available at https://github.com/mamedev/mame

## Ericsson material

**No Ericsson firmware, ROM image, diskette image or documentation is included
in this repository.**

The Alfaskop 91 and the Alfaskop System 41 hardware, firmware and software are
the work of Ericsson Information Systems AB. References to part numbers,
addresses, register layouts and observed behaviour are descriptions of a
historical system, made for interoperability and preservation.

To run the emulation you need the ROM set and the diskette images, obtained
separately.
