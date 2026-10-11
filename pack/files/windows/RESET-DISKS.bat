@echo off
rem Close the emulator and put the working diskettes back as the originals
cd /d "%~dp0"
call "%~dp0STOP.bat" >nul
copy /y "%~dp0disks\originals\A91R4A_1.IMD" "%~dp0disks\" >nul
copy /y "%~dp0disks\originals\A91R4A_2.IMD" "%~dp0disks\" >nul
copy /y "%~dp0disks\originals\A91SETUP.IMD" "%~dp0disks\" >nul
echo Working diskettes restored from disks\originals.
pause
