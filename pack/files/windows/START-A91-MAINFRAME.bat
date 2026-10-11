@echo off
rem Alfaskop 91 + DU 4110 on an emulated IBM mainframe: the 3705 with its NCP, and MVS 3.8j with TSO
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0mainframe\start-mainframe.ps1" %*
