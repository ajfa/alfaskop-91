# run-windows.ps1 [seconds] - Windows counterpart of run.sh: start the 3705 and TK5's Hercules (no windows), IPL,
# load NCP N16A and run the Alfaskop 91 in a window with its host line on 3705 line 020, activating the line,
# the PU and the LU once the A91 has taken its call. With $env:LUA set there is no window and the Lua script runs.
# Locations, as in run.sh: $env:A91_MAME (a91.exe), $env:A91_ROMPATH, $env:A91_DISKS; TK5 is $env:TK5 or
# ..\work\mvs-tk5, and build-windows.sh's output is $env:WORK_WINDOWS or ..\work\windows.
param([int]$Seconds = 0)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
function Opt($name, $default) { $v = [Environment]::GetEnvironmentVariable($name); if ($v) { $v } else { $default } }
$tk5  = (Resolve-Path (Opt 'TK5' (Join-Path $here '..\work\mvs-tk5'))).Path
$win  = (Resolve-Path (Opt 'WORK_WINDOWS' (Join-Path $here '..\work\windows'))).Path
$mame = (Resolve-Path (Opt 'A91_MAME' 'a91.exe')).Path
$roms = (Resolve-Path (Opt 'A91_ROMPATH' 'roms')).Path
$disks = (Resolve-Path (Opt 'A91_DISKS' '.')).Path
$lua = $env:LUA
$run = Join-Path $win 'run'
$hlog = Join-Path $run 'herc.log'
New-Item -ItemType Directory -Force $run | Out-Null

function Stop-Ours {
	Get-Process hercules, i3705 -ErrorAction SilentlyContinue |
		Where-Object { $_.Path -and $_.Path.StartsWith($win, [StringComparison]::OrdinalIgnoreCase) } |
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

function Lines { if (Test-Path $hlog) { @(Get-Content $hlog -ErrorAction SilentlyContinue).Count } else { 0 } }

function Wait-Log($pattern, $secs, $from) {
	for ($i = 0; $i -lt $secs; $i++) {
		Start-Sleep 1
		$lines = @(Get-Content $hlog -ErrorAction SilentlyContinue)
		if ($lines.Count -gt $from -and ($lines[$from..($lines.Count - 1)] -match $pattern)) { return $true }
	}
	return $false
}

# .NET may start the process's input with a UTF-8 BOM (from a UTF-8 console), and Hercules would then take the
# first MVS command for one of its own: an empty line goes first to carry it, and commands go out in ASCII
function Mvs($cmd) { $script:hin.WriteLine("/$cmd") }

$herc = $null
try {
	Stop-Ours
	if (-not (Get-ChildItem (Join-Path $env:windir 'WinSxS') -Filter 'amd64_microsoft.vc90.crt_*' -Directory -ErrorAction SilentlyContinue)) {
		throw "TK5's Hercules needs the 64-bit Visual C++ 2008 runtime (vcredist_x64 2008)"
	}
	Remove-Item $hlog -ErrorAction SilentlyContinue
	'starting the 3705 and MVS (about 2 minutes)'
	Start-Hidden (Join-Path $win 'i3705\usr\bin\i3705.exe') '3705-128k.cnf' (Join-Path $win 'i3705') (Join-Path $run 'i3705.log') @{ I3705_LINE_ADDR = '127.0.0.1' } | Out-Null
	Start-Sleep 2
	$herc = Start-Hidden (Join-Path $win 'hercules\hercules.exe') '-d -f conf\tk5.cnf' $tk5 $hlog @{ HERCULES_RC = 'scripts\ipl.rc'; TK5CRLF = 'CRLF' }
	$script:hin = New-Object System.IO.StreamWriter($herc.StandardInput.BaseStream, (New-Object System.Text.ASCIIEncoding))
	$script:hin.AutoFlush = $true
	$script:hin.WriteLine('')
	if (-not (Wait-Log 'IKT005I' 600 0)) { throw "MVS did not come up, see $hlog" }
	Start-Sleep 8
	$n = Lines; Mvs 'v net,act,id=N16A'
	if (-not (Wait-Log 'IST093I  N16A +ACTIVE' 300 $n)) { throw "N16A did not load, see $hlog" }
	'NCP N16A loaded'

	# work on copies of the diskettes; snapshots and the logon log land here too
	$a = Join-Path $run 'a91'
	if (Test-Path $a) { Get-ChildItem $a -Recurse -File | Remove-Item -Force }
	New-Item -ItemType Directory -Force $a | Out-Null
	Copy-Item (Join-Path $disks 'A91R4A_1.IMD'), (Join-Path $disks 'A91R4A_2.IMD') $a
	$args91 = @('a91du', '-rompath', $roms, '-flop1', 'A91R4A_1.IMD', '-flop2', 'A91R4A_2.IMD',
		'-bitb', 'socket.127.0.0.1:37520', '-skip_gameinfo', '-snapshot_directory', $a)
	if ($Seconds -gt 0) { $args91 += @('-seconds_to_run', $Seconds) }
	if ($lua) { $args91 += @('-video', 'none', '-sound', 'none', '-autoboot_script', (Resolve-Path $lua).Path) }
	else { $args91 += @('-window', '-nomaximize') }
	$a91 = Start-Process -FilePath $mame -ArgumentList ($args91 | ForEach-Object { if ("$_" -match ' ') { "`"$_`"" } else { "$_" } }) -WorkingDirectory $a -PassThru
	'Alfaskop 91 started; the network calls it at about 52 seconds'

	# the NCP has no use for the line until the A91 has answered the call and its first XID
	Start-Sleep ([int](Opt 'ACT_DELAY' 57))
	foreach ($id in 'L16A20', 'P16A20A', 'T16A20A1') {
		$n = Lines; Mvs "v net,act,id=$id"
		if (Wait-Log "IST093I  $id +ACTIVE" 20 $n) { "$id active" } else { "$id not active, see $hlog" }
	}
	$a91.WaitForExit()
}
finally {
	if ($herc -and -not $herc.HasExited -and -not $env:KEEP) {
		try { $script:hin.WriteLine('quit') } catch { }
		$herc.WaitForExit(60000) | Out-Null
	}
	if (-not $env:KEEP) { Stop-Ours }
}
