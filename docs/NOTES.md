# Alfaskop 91 hardware and firmware notes

What the driver relies on, and where each piece came from. Addresses are
physical 68000 addresses unless stated. "Inferred" means read from the firmware
without a schematic or a measurement on a board.

## Boards

Main board E340587091/000-2: MC68000G8, HD68450-8 DMA, MK68564N-4A dual serial,
MC68230P8 PI/T, WD2797A-PL floppy controller, TMS4500A DRAM controller.
TCC board E340587093: MC6800P, MC6840P timer, MC6844P DMA, four MC6854P ADLCs
and static RAM, with no EPROM of its own.

## Boot ROM

Two 8 KiB EPROMs, the even and odd byte lanes of one 16 KiB 68000 ROM at
`c00000`:

| dump | lane | CRC32 | SHA1 |
|---|---|---|---|
| `E34058709112202_Fujitsu_MBM2764_CH000FC8E4.bin` | even, D8-D15 | `fff22d87` | `e4f15b31d076e0141bacda68fd89cf828de09dff` |
| `E34058709112302_Fujitsu_MBM2764_CH0011F7C0.bin` | odd, D0-D7 | `852c3c67` | `6c5ba633ce4fb604b4b6bf8794d4dadb8a811ad4` |

The CH value in each name is the byte sum. Initial SP `0000037c`, initial PC
`ffc006a2`, identification `E34058909112212302` at offset `000a`. The checksum
routine at `c00ba6` is a reflected CRC, polynomial `a001`, initial value zero,
bytes exchanged; over `000a` to `3ffe` it gives `e129`, the word at `0008`.

| ROM location | purpose |
|---|---|
| `c00040-c0009f` | jump table for disk-loaded software |
| `c0005e` (`c0013e`) | front panel service, see below |
| `c00082` (`c00322`) | front panel tick, called by CP91OS |
| `c006a2` | reset entry |
| `c00728-c00786` | probes expansion RAM and optional boards through bus errors |
| `c008bc-c008ea` | finds the end of RAM in 128 KiB steps |
| `c00abc` | RAM pattern test |
| `c00bf2` | ROM CRC test |
| `c00c3a` | DMA, serial, PI/T and FDC initialisation |
| `c00fd4` | floppy, DMA and timer interrupt handler |
| `c016dc` | boot disk search |
| `c019a2` | loads the bootstrap and calls it |

## Main CPU map

| address | what |
|---|---|
| `000000-07ffff` | DRAM. 512 KiB: the 3270 emulation diskette loads PHYSLIB and X21EMUL up to `068000` |
| `c00000-c03fff` | boot ROM |
| `f08000-f0bfff` | 16 KiB RAM shared with the TCC 6800 |
| `f0fff0-f0ffff` | TCC control: reset vector, reset release, mailbox |
| `ff8000-ff80ff` | HD68450, channel 0 line 1 receive, channel 1 line 1 transmit, channel 3 floppy |
| `ff8801-ff883f`, odd | MC68230 PI/T |
| `ff8a01-ff8a3f`, odd | MK68564, channel A is host line 1 |
| `ff8c00-ff8c03` | X.21 adapter for line 1 (inferred) |
| `ff9001-ff9007`, odd | WD2797 |

Unmapped accesses raise bus errors, which the ROM uses to probe for optional
boards.

## Interrupts

All autovectored.

| level | source |
|---|---|
| 1 | software: PI/T PC7 low. The level 2 handlers clear it to defer work, the level 1 handler sets it again. Without it pSOS has no tick |
| 2 | floppy, DMA and PI/T timer; also the TCC mailbox at `f0fff1` |
| 3 | X.21 adapter, pending when bit 7 of `ff8c01` is set |
| 4 | MK68564. The handler at `7b4c` reads the vector register at `ff8a19`; vectors `68`-`71` |

## PI/T

* Port A: bits 0-1 motor enables, active low; bits 2-3 drive selects, which
  FD91OS leaves both low; bit 4 selects the drive (0 first, 1 second); bit 7
  reads the FDC interrupt. Inferred from the boot drive selection.
* H1 and H2: diskette present. FD91OS mounts a drive only if its level reads 1
  at start; the ROM toggles the matching sense bit after each change.
* Port B: the front panel, below.
* Port C: PC7 is the level 1 software interrupt. PC2 follows a configuration
  option in INIT1 and is not related to the display.

## Front panel

Port B, all outputs, drives two decimal digits in BCD, high nibble on the left;
`ff` blanks them. Evidence: the count during loading goes from `09` to `10`.

The ROM service at `c0005e` takes `d0.w` = command byte, code byte:

| command | effect |
|---|---|
| `00` | store the code in its slot, the slot being the first digit (0-9) |
| `ff` | clear that slot |
| `a2` | write the code straight to the display |
| `a1`, `a0` | lock and unlock |
| `a3` | reset the slot table |
| `fd` | show the code blinking forever with interrupts masked (fatal) |

The tick routine rotates the active slots, each one second on and one second
blank, and shows `00` when none is active. Measured boot: `03 04 05` from the
ROM, `06` to `20`, `30`, `00` while the system loads, then `60 61 50 51`
rotating until the session is up, then `00`. Fatal codes present in the code:
`04`, `05` (CP91OS), `64`, `74` (X.21 emulation). The decoder that blanks on
`F` is a guess.

## TCC board

| 6800 address | what |
|---|---|
| `0000-3fff`, mirrored at `4000-7fff` | local 16 KiB RAM |
| `8000-bfff` | the RAM shared with the 68000 |
| `c000-c016` | MC6844 |
| `c020-c027` | MC6840 |
| `c040-c04f` | the four ADLCs, four bytes apart |
| `fff0-ffff` | mailbox and vectors |

`FD91BOOT` loads `TCC91OS` at `f0800c`, writes that address to `f0fffc` as the
6800 reset vector and `0080` to `f0fff4` to release reset. The 6800 tests its
RAM and reports `02` in shared byte `8002`. TCC91OS uses MC6844 mode 2 (halt),
which requests the bus on DRQ2, so both DRQ1 and DRQ2 are granted.

The four ADLCs share the same two-wire pair. TCC91OS polls on channel 3 and
runs the IPL and file sessions on channel 2. The display unit hears every
channel and its answer reaches every armed receiver.

## Diskettes

80 cylinders, two heads, nine sectors of 512 bytes, MFM. The setup diskette
has physical sector order `1,6,2,7,3,8,4,9,5`; mount the original IMD files,
which keep it.

| image | label | contents |
|---|---|---|
| `A91R4A_1.IMD` | `S92230011SYST`, `V5.6-4A` | system: FD91BOOT, TCC91OS, CP91NIP, CP91VECT, FD91OS, CP91OS, CPBASOS and the libraries |
| `A91R4A_2.IMD` | `D92230012EM` | the 3270 SNA switch emulation: libraries C9A (CPEMUL, X21EMUL), PHYSLIB (X21, INIT1), MANLIB, SYSPARAM and others |
| `A91SETUP.IMD` | `S46030712SYST`, `M506-02` | customer setup and test |

`tools/a91fs.py` lists and extracts files and library members from a flat
image. A directory entry is 32 bytes; its CHS field points at the sector before
the data.

The setup diskette's configuration program (`D7GCSU`: VGAPGM, VGADATA, SYMEM)
is for the Alfaskop 9014 VGA display. On a DU 4110 only the manual load line is
usable; loading that program hangs the display unit.

## DU 4110 on the A91

The display unit is the System 41 DU 4110 from `alfaskop41xx.cpp`, attached to
TCC line 3. The A91 sends it DUOS over the line in about 55 seconds, then the
emulation programs.

Keyboard type. The DUOS sends order `88` to the keyboard and compares the
returned ROM-ID with `E34058 7037` (KBU 4143) and `E34058 7058` or
`CAR 102 0042` (KBU 9140); `40...` means KBU 4140. It then asks the A91 for the
library `KG<type>`. The R4A diskettes carry only `KG4143`, `KG9140` and
`K79140`, so the emulated keyboard presents the 4143 ROM-ID with its CRC-16
(polynomial `a001`, initial value zero, over `F802-FFFF`, stored at `F800`)
recomputed.

Characters. DUOS 5.6 writes Ericsson's 8-bit internal code to video RAM (`C3`
is Ä, `F3` is ö and so on), while the DU's character generator is the 7-bit
Swedish set. The driver maps the twelve codes that the third part of library
`L00010A` defines; the currency sign shows as `$` because the generator has `$`
there. 3270 field attributes are `0x80 | attribute`, with bit 1 always zero.

Keys. The keyboard controller sends `80 nn`, `nn` = column * 8 + row, 0-127,
which indexes the four planes of table `P000101`. Shift Lock (24), Shift (32)
and Alt (108) are loaded into the controller by the DUOS and come back as bits
of the first byte. In session: ENTER `421f`, NEW LINE `420d`, CLEAR `4220`,
ERASE INPUT `420c`, PF1-12 `43f1`-`43f9`, `437a`-`437c`, PF13-24 `43c1`-`43c9`,
`434a`-`434c`, PA1-3 `416c`, `416e`, `416b`.

## Host line

Line 1 is MK68564 channel A with HD68450 channels 0 (receive) and 1 (transmit),
device address `ffff8a13`, and the X.21 adapter at `ff8c00`. The product is
"ALFASKOP 91 Single Line IBM 3270 SNA switch 5.6H" for switched X.21 (Datex).

X.21 adapter, inferred from PHYSLIB:

* write `ff8c01`: bit 6 selects the line register at `ff8c03`, bit 1 enables
  the interrupt;
* line register: bit 0 C, bit 1 T held at 1, bit 2 T from the MK68564, bit 7
  enables the R/I detector. `82` ready, `83` accept a call, `85` data;
* read `ff8c01`: `c9` R=1 with I off, `ca` R=0, `cc` R alternating 0101; `c0`
  with bit 7 of `ff8c03` is R=1 with I on, ready for data;
* bit 4 of `ff8c03` toggles at each closing flag. PHYSLIB adds a byte to the
  received length when this bit is the same at the first character and at the
  end of the frame. This is the least certain part.

The A91 does not dial. The network calls it: SYN SYN BEL on R. The A91 accepts
with `83`, the DCE signals connection in progress and then ready for data, and
SDLC starts.

SDLC: secondary station `c1`. The first frame must be a format 1 XID (first
byte `1x`); otherwise every frame gets DM. The A91's XID is
`12 1a 0173 7907 ... 06 f1f7f0f3f9f8` (IDBLK 017, its number 170398).

SNA: FID2. ACTPU, then ACTLU for LUs 2 to 5. When the display unit has finished
loading, the A91 sends NOTIFY `810620`; a BIND before that gets `-RSP 0845`.
LU2 BIND, then SDT. The first data request is `03 80 a0` (DR1, BB, CD, no ERI;
ERI gives `-RSP 2003`); later ones `03 80 20`, since BB again gives `0813`.
Normal flow sequence numbers start at 1 after SDT and the expedited flow has
its own counter. The A91 speaks EBCDIC code page 278 through library `L000101`.

## Determinism

Runs must be identical, and several MAME devices read state they never wrote.
Found and fixed here: the MC6844 channel state and grant line, the MC68230 port
input lines, the MC6846 state before reset, and the z80sio receiver state,
including the receive CRC accumulator. The last one made about one Windows boot
in fifteen fail: with a non-zero accumulator every synchronous character carries
the CRC error bit, the A91's Error Reset then drops the next character, and that
character was the BEL of the incoming X.21 call.
