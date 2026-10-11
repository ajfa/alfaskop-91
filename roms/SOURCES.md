# ROM images

One directory per machine or device, with MAME's file names, so that
`-rompath roms` works as it is. With the patch in `patches/`,
`-verifyroms a91 a91du alfaskop4110 alfaskop4120 alfaskop4101` reports all five
sets good.

## The A91 program EPROMs

`a91/e34058709112202.bin` and `a91/e34058709112302.bin`, the two Fujitsu
MBM2764 EPROMs of the A91 CPU board, high and low byte. **Mattis Lind** dumped
them from his own A91 and sent them by e-mail on 2026-10-02 as
`E34058709112202_Fujitsu_MBM2764_CH000FC8E4.bin` and
`E34058709112302_Fujitsu_MBM2764_CH0011F7C0.bin`; the `CH` value is the byte
sum, and both match.

| file | SHA-1 |
|---|---|
| `e34058709112202.bin` | `e4f15b31d076e0141bacda68fd89cf828de09dff` |
| `e34058709112302.bin` | `6c5ba633ce4fb604b4b6bf8794d4dadb8a811ad4` |

## The System 41 ROMs

From **Mattis Lind**'s preservation work,
<https://github.com/MattisLind/alfaskop_emu>, renamed to MAME's names:

| file | from | what it is |
|---|---|---|
| `e3405870205201.bin` | `hardware/DU4110` | DU 4110 IPL PROM, DTC-A IC52 |
| `e3405972067500.bin`, `e3405972067600.bin` | `hardware/DU4110` | DU 4110 character generator, IC75 and IC76 |
| `dtc_a_e34062_0100_ic50_e3405970303601.bin` | `hardware/DU4110` | DU 4110 Signetics 82S100 |
| `kbc_e34066_0000_ic3.bin`, `kbc_e34066_0000_ic4_e3405970280400.bin` | `hardware/DU4110` | keyboard |
| `e3405870034703.bin` | `hardware/CPR4101` | CPR 4101, CPB IC47 |
| `e3405870080004.bin`, `e3405870150103.bin` | `hardware/FD4120` | FD 4120, FDP IC43 and IC44 |

`a91/` also holds copies of the DU 4110 and keyboard ROMs, as the packs ship
them.
