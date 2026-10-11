# gate-windows.ps1 <zip> [-Mainframe] - check the Windows pack as it is delivered: unzip it into a fresh folder
# with a space in its name, parse its PowerShell, and run START-A91.bat hidden and headless with tools\gate.lua
# (the host echoes what is typed). With -Mainframe it also runs the mainframe launcher with host3705\tso-logon.lua.
param([Parameter(Mandatory = $true)][string]$Zip, [switch]$Mainframe)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot
$g = Join-Path $env:TEMP ('a91 gate ' + (Get-Date -Format 'HHmmss'))
Expand-Archive -Path $Zip -DestinationPath $g
$p = Join-Path $g 'Alfaskop91-DU4110'
$rc = 0

foreach ($f in Get-ChildItem $p -Recurse -Filter '*.ps1') {
	$errs = $null
	[System.Management.Automation.Language.Parser]::ParseFile($f.FullName, [ref]$null, [ref]$errs) | Out-Null
	if ($errs) { "PARSE ERROR in $($f.Name): $($errs[0].Message)"; $rc = 1 }
}
foreach ($f in Get-ChildItem $p -Filter '*.bat') {
	if ((Get-Content $f.FullName -Raw) -match "[^\r]`n") { "NOT CRLF: $($f.Name)"; $rc = 1 }
}

function Run-Hidden($bat, $more, $out, $secs) {
	$a = '/c', "`"`"$p\$bat`" -video none -sound none -seconds_to_run $secs $more > `"$p\$out`" 2>&1`""
	Start-Process -FilePath cmd.exe -ArgumentList $a -WorkingDirectory $p -WindowStyle Hidden -PassThru -Wait | Out-Null
}

Run-Hidden 'START-A91.bat' "-nothrottle -autoboot_script `"$repo\tools\gate.lua`"" 'gate-normal.txt' 110
$res = Join-Path $p 'gate-result.txt'
if (Test-Path $res) { 'normal: ' + (Get-Content $res -TotalCount 1) } else { 'normal: no gate-result.txt' }
if (-not ((Test-Path $res) -and ((Get-Content $res -TotalCount 1) -eq 'PASS'))) { $rc = 1 }

if ($Mainframe -and (Test-Path (Join-Path $p 'START-A91-MAINFRAME.bat'))) {
	$t0 = Get-Date
	Run-Hidden 'START-A91-MAINFRAME.bat' "-autoboot_script `"$repo\host3705\tso-logon.lua`"" 'gate-mainframe.txt' 300
	$tso = Join-Path $p 'tso.txt'
	$last = if (Test-Path $tso) { (Select-String -Path $tso -Pattern 'TSO OK' | Select-Object -Last 1).Line } else { 'no tso.txt' }
	'mainframe: {0:N0} s, {1}' -f ((Get-Date) - $t0).TotalSeconds, $last
	if ($last -notmatch 'TSO OK') { $rc = 1 }
}

$left = Get-Process a91, hercules, i3705 -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($p) }
if ($left) { 'left running: ' + ($left.Name -join ', '); $rc = 1 }
foreach ($d in 'A91R4A_1', 'A91R4A_2', 'A91SETUP') {
	$a = (Get-FileHash (Join-Path $p "disks\originals\$d.IMD") -Algorithm SHA1).Hash
	$b = (Get-FileHash (Join-Path $repo "media\mattis-lind\$d.IMD") -Algorithm SHA1).Hash
	if ($a -ne $b) { "originals changed: $d"; $rc = 1 }
}
"gate dir: $g"
"gate rc=$rc"
exit $rc
