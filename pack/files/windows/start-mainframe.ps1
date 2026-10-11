# start-mainframe.ps1 - Alfaskop 91 on an emulated IBM mainframe: the 3705 with its NCP and MVS 3.8j (TK5) on
# Hercules, with no window; the A91 in its own window. START-A91-MAINFRAME.bat calls it.
# Extra arguments go to a91.exe as they are.
$ErrorActionPreference = 'Stop'
$M = $PSScriptRoot
$P = Split-Path $M
$log = Join-Path $M 'log'
$hlog = Join-Path $log 'hercules.log'
New-Item -ItemType Directory -Force $log | Out-Null

function Stop-Pack {
	Get-Process a91, hercules, i3705 -ErrorAction SilentlyContinue |
		Where-Object { $_.Path -and $_.Path.StartsWith($P, [StringComparison]::OrdinalIgnoreCase) } |
		Stop-Process -Force -ErrorAction SilentlyContinue
}

function Start-Hidden($exe, $arguments, $dir, $out, $envs) {
	$s = New-Object System.Diagnostics.ProcessStartInfo
	$s.FileName = 'cmd.exe'
	$s.Arguments = "/c `"`"$exe`" $arguments > `"$out`" 2>&1`""
	$s.WorkingDirectory = $dir
	$s.UseShellExecute = $false
	$s.RedirectStandardInput = $true
	$s.CreateNoWindow = $true
	foreach ($k in $envs.Keys) { $s.EnvironmentVariables[$k] = $envs[$k] }
	return [System.Diagnostics.Process]::Start($s)
}

function Wait-Log($pattern, $secs, $from) {
	for ($i = 0; $i -lt $secs; $i++) {
		Start-Sleep 1
		if (Test-Path $hlog) {
			$lines = @(Get-Content $hlog -ErrorAction SilentlyContinue)
			if ($lines.Count -gt $from -and ($lines[$from..($lines.Count - 1)] -match $pattern)) { return $true }
		}
		if ($i % 10 -eq 9) { Write-Host -NoNewline '.' }
	}
	return $false
}

function Lines { if (Test-Path $hlog) { @(Get-Content $hlog -ErrorAction SilentlyContinue).Count } else { 0 } }

function Mvs($herc, $cmd) { $herc.StandardInput.WriteLine("/$cmd"); $herc.StandardInput.Flush() }

$herc = $null
try {
	Stop-Pack
	if (-not (Get-ChildItem (Join-Path $env:windir 'WinSxS') -Filter 'amd64_microsoft.vc90.crt_*' -Directory -ErrorAction SilentlyContinue)) {
		throw "Microsoft's 64-bit Visual C++ 2008 library (vcredist_x64 of 2008), which Hercules uses, is missing"
	}
	Remove-Item $hlog -ErrorAction SilentlyContinue
	Write-Host 'Alfaskop 91 on an IBM mainframe: 3705 communications controller and MVS 3.8j (TK5).'
	Write-Host ''
	Write-Host 'Starting the 3705 and MVS, about 2 minutes. Nothing needs to be typed in this window.'
	$i3705 = Start-Hidden (Join-Path $M 'i3705\usr\bin\i3705.exe') '3705-128k.cnf' (Join-Path $M 'i3705') (Join-Path $log 'i3705.log') @{ I3705_LINE_ADDR = '127.0.0.1' }
	Start-Sleep 2
	$herc = Start-Hidden (Join-Path $M 'hercules\hercules.exe') '-d -f conf\tk5.cnf' (Join-Path $M 'tk5') $hlog @{ HERCULES_RC = 'scripts\ipl.rc'; TK5CRLF = 'CRLF' }
	if (-not (Wait-Log 'IKT005I' 600 0)) { throw "MVS did not finish starting; see $hlog" }
	Start-Sleep 8
	Write-Host ''
	Write-Host 'MVS is up. Loading the NCP into the 3705, about 40 seconds.'
	$n = Lines; Mvs $herc 'v net,act,id=N16A'
	if (-not (Wait-Log 'IST093I  N16A +ACTIVE' 300 $n)) { throw "The NCP did not load; see $hlog" }
	Write-Host ''
	Write-Host 'NCP loaded. Opening the Alfaskop 91.'

	$d = Join-Path $P 'disks'
	foreach ($f in 'A91R4A_1.IMD', 'A91R4A_2.IMD') {
		if (-not (Test-Path (Join-Path $d $f))) { Copy-Item (Join-Path $d "originals\$f") $d }
	}
	$a91args = @('a91du', '-rompath', (Join-Path $P 'roms'), '-flop1', (Join-Path $d 'A91R4A_1.IMD'),
		'-flop2', (Join-Path $d 'A91R4A_2.IMD'), '-bitb', 'socket.127.0.0.1:37520', '-window', '-nomaximize',
		'-skip_gameinfo', '-snapshot_directory', (Join-Path $P 'snapshots'), '-cfg_directory', (Join-Path $P 'cfg')) + $args
	$a91 = Start-Process -FilePath (Join-Path $P 'a91.exe') -ArgumentList ($a91args | ForEach-Object { if ($_ -match ' ') { "`"$_`"" } else { $_ } }) -WorkingDirectory $P -PassThru

	# the network calls the A91 at about 52 seconds; then the line, the PU and the LU are activated
	Start-Sleep 57
	foreach ($id in 'L16A20', 'P16A20A', 'T16A20A1') {
		$n = Lines; Mvs $herc "v net,act,id=$id"
		if (Wait-Log "IST093I  $id +ACTIVE" 20 $n) { Write-Host "$id active" } else { Write-Host "$id did not become active; see $hlog" }
	}
	Write-Host ''
	Write-Host 'Ready. In the A91 window, with the VTAM screen and SYS OP on the status line:'
	Write-Host '  LOGON APPLID(TSO) LOGMODE(MHP3278E)  and Enter'
	Write-Host '  if X 443 shows on the status line: left Ctrl (RESET) and F3'
	Write-Host '  HERC01, Enter; CUL8TR, Enter; Enter at the first ***; Page Up (PA1) at the ***'
	Write-Host '  of the fortune cookie; at READY, any TSO command (LISTCAT, LOGOFF)'
	Write-Host ''
	Write-Host 'Closing the A91 window shuts down MVS and the 3705.'
	$a91.WaitForExit()
}
catch {
	Write-Host ''
	Write-Host "ERROR: $($_.Exception.Message)"
	Read-Host 'Press Enter to close'
}
finally {
	if ($herc -and -not $herc.HasExited) {
		try { $herc.StandardInput.WriteLine('quit'); $herc.StandardInput.Flush() } catch { }
		$herc.WaitForExit(60000) | Out-Null
	}
	Stop-Pack
}
