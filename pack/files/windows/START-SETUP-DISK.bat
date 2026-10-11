@echo off
rem Alfaskop 91 + DU 4110 with the A91SETUP diskette (system M5.06-02); it only gets to the manual load line
cd /d "%~dp0"
if not exist "%~dp0disks\A91SETUP.IMD" copy /y "%~dp0disks\originals\A91SETUP.IMD" "%~dp0disks\" >nul
"%~dp0a91.exe" a91du -rompath "%~dp0roms" -flop1 "%~dp0disks\A91SETUP.IMD" -window -nomaximize -skip_gameinfo -snapshot_directory "%~dp0snapshots" -cfg_directory "%~dp0cfg" %*
