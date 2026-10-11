@echo off
rem Close the emulator, Hercules and the 3705 of this pack if they were left running (only this pack's)
powershell -NoProfile -Command "Get-Process a91,hercules,i3705 -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '%~dp0*' } | Stop-Process -Force"
echo Emulator closed.
