# The packs

Self-contained folders, one for Linux and one for Windows, that run the A91 with
a double-click or one command: the emulator, the ROMs, the three diskettes the
A91 boots from, launchers and a step-by-step `README.txt`. Each one also has the
mainframe mode: the IBM 3705 emulator, Hercules and the TK5 MVS that
`host3705/` prepares, so the DU 4110 logs on to TSO.

## Linux

1. Build `a91` as in the top [README](../README.md).
2. For the mainframe mode, run `host3705/prepare.sh` and `host3705/setup-tk5.sh`
   (see [host3705/README.md](../host3705/README.md)). Without it, set `MAINFRAME=0`.
3. Build and check the pack:

```sh
A91_BIN=/path/to/mame/a91 ./build-linux.sh
./gate-linux.sh out/Alfaskop91-DU4110-linux.zip
```

`build-linux.sh` takes the SDL2 and SDL2_ttf libraries from the build machine
and puts them in `lib/`. The 3705 emulator, Hercules and TK5 come from
`host3705/work`, or from `I3705`, `HERC` and `TK5` if set. The gate runs both
launchers headless and passes when the host echoes the typed line and TSO
answers the logon.

## Windows

1. Build `a91.exe` with MSYS2's MinGW64 compiler: `build-a91-windows.sh`, from a
   MAME 0.288 tree with the patch applied.
2. For the mainframe mode, run `host3705/windows/build-windows.sh` on a TK5
   prepared as for Linux. Without it, set `MAINFRAME=0`.
3. Build the pack (from WSL, Linux or Git Bash) and check it (in PowerShell):

```sh
A91_EXE=/path/to/a91.exe ./build-windows.sh
```

```powershell
.\gate-windows.ps1 out\Alfaskop91-DU4110-windows.zip
```

The gate parses the pack's PowerShell, checks the line ends of the `.bat`
files and runs `START-A91.bat` hidden and headless. Hercules on Windows needs
Microsoft's 64-bit Visual C++ 2008 runtime; the mainframe launcher says so if
it is missing.

## Files

| file | what it does |
|---|---|
| `build-linux.sh`, `build-windows.sh` | lay out the pack and zip it |
| `build-a91-windows.sh` | builds `a91.exe` with MSYS2 MinGW64 |
| `readme.py`, `files/README.txt.in` | the pack's `README.txt`, for each system |
| `files/START-A91-MAINFRAME.sh`, `files/windows/` | the launchers that are not generated |
| `mkzip.py` | zips a pack; on Linux it keeps the Unix modes |
| `gate-linux.sh`, `gate-windows.ps1` | unzip a pack and run it headless |
