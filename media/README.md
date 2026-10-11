# Diskette images and photographs

Everything in this directory was read from original media by other people and
given to this project. The bytes are as they arrived; only the directories
they were packed in were removed. Each directory has a `SHA1SUMS`.

The software on these diskettes belongs to Ericsson Information Systems AB. It
is here for preservation and so that the emulation can be run and checked; see
[NOTICE.md](../NOTICE.md).

## mattis-lind/

ImageDisk images that **Mattis Lind** read from his own Alfaskop diskettes
between 2022 and 2024 (the dates are in the IMD headers), with
`A91_disks.csv`, his transcription of the labels and of the bad sectors he
found. He gave them to this project on 2026-10-03 (`A91R4A_1`, `A91R4A_2` and
`A91SETUP`, by e-mail) and on 2026-10-04 (the whole set). The three sent by
e-mail are byte for byte the same as the ones in the set and are stored once.

| image | label |
|---|---|
| `A91R4A_1.IMD` | ALFASKOP 91, SNA Switch, 9223-0011 NSE, system, V5.6 8806, R4A. `a91du` boots it |
| `A91R4A_2.IMD` | ALFASKOP 91, Single Line IBM 3270, SNA Switch, 9223-0012 NSE, emulation, V5.6 8806 |
| `A91SETUP.IMD` | Alfaskop A91 Customer Setup Test Diskette V5.6, 9200-002, 88-10-25 |
| `A91R3A_1.IMD`, `A91R3A_2.IMD` | ALFASKOP 91, Single Line, SNA leased, 9222-0011 and 9222-0012 EIV, V5.4 870116 |
| `A91_1` to `A91_14`, `A10B`, `A11B` | Alfaskop 91 releases from V5.1 (1985) to V5.3 (1986) and System 41 diskettes; see the CSV |
| names ending in `B` or `C` | further reads of the same diskettes, as Mattis named them |
| the two long names | the labels written out as file names |

## poul-henning-kamp/

Read by **Poul-Henning Kamp** with Datamuseum.dk's FloppyTools, which the IMD
headers name. The eight-digit numbers in the file names are the ones the
images came with.

| directory | what it is |
|---|---|
| `5inch-2026-09-09/` | 13 images of 5-inch Alfaskop diskettes, sent on 2026-09-09 |
| `8inch-2026-10-04/` | 14 images of 8-inch Alfaskop diskettes, sent on 2026-10-04 |
| `photos-2026-10-04/` | 29 photographs sent with the 8-inch set; the first shows the cover of Stansaab's *Alfaskop 3500 Data Terminal System, SAS Technical Description* |

## Using them

Work on copies. The emulator writes to the diskette images it is given, and
the originals here should stay as they came.
