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

## host3705: the IBM 3705 emulator, Hercules, TK5 and IBM software

The scripts in `host3705/` are BSD-3-Clause like the rest of this repository.
They download and build software that is **not** included here and keeps its
own license:

* the IBM 3705 emulator, https://github.com/snhstq/IBM3705_R5, by Edwin
  Freekenhorst and Henk Stegeman (GPL-3.0), including `comm3705.c` for
  Hercules and the NCP volume `ncpssp.3350`;
* Hercules, https://github.com/SDL-Hercules-390/hyperion (Q Public License);
* TK5, the MVS 3.8j turnkey system by Rob Prins, which the user downloads.

MVS 3.8j, VTAM, TSO, the NCP and the other IBM programs on those volumes are
IBM's. The jobs built by `tk5jobs.py` change members of the user's own TK5
system; no IBM source or member is copied into this repository.
`host3705/jcl/ncpgen1.jcl` follows the sample job `NCPGEN` on the NCP volume.

## Ericsson material

**No Ericsson firmware, ROM image, diskette image or documentation is included
in this repository.**

The Alfaskop 91 and the Alfaskop System 41 hardware, firmware and software are
the work of Ericsson Information Systems AB. References to part numbers,
addresses, register layouts and observed behaviour are descriptions of a
historical system, made for interoperability and preservation.

To run the emulation you need the ROM set and the diskette images, obtained
separately.
