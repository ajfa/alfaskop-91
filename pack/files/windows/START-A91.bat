@echo off
rem Alfaskop 91 + DU 4110 with the R4A diskettes (system and 3270 emulation) and the emulated host
cd /d "%~dp0"
if not exist "%~dp0disks\A91R4A_1.IMD" copy /y "%~dp0disks\originals\A91R4A_1.IMD" "%~dp0disks\" >nul
if not exist "%~dp0disks\A91R4A_2.IMD" copy /y "%~dp0disks\originals\A91R4A_2.IMD" "%~dp0disks\" >nul
"%~dp0a91.exe" a91du -rompath "%~dp0roms" -flop1 "%~dp0disks\A91R4A_1.IMD" -flop2 "%~dp0disks\A91R4A_2.IMD" -window -nomaximize -skip_gameinfo -snapshot_directory "%~dp0snapshots" -cfg_directory "%~dp0cfg" %*
