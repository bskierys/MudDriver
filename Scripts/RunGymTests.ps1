<#
runs functional tests (gym rigs) headless and prints the B1Test result lines.
exit code is 0 only when at least one test ran and every test passed; the editor's
own exit code ignores test results, so the log is parsed instead.

  .\Scripts\RunGymTests.ps1
  .\Scripts\RunGymTests.ps1 -Map /Game/B1/Maps/Lvl_Gym_Mud -Filter FullThrottle
#>
param(
    [string]$Map = "/Game/Variant_OffRoad/Maps/Lvl_Offroad",
    [string]$Filter = "",
    [int]$TimeoutSec = 900,
    [string]$EngineDir = "C:\Program Files\UE_5.8"
)

$ProjectDir = Resolve-Path (Join-Path $PSScriptRoot "..")
$Project = Join-Path $ProjectDir "MudDriver.uproject"
$Editor = Join-Path $EngineDir "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$LogName = "RunGymTests.log"
$LogPath = Join-Path $ProjectDir "Saved\Logs\$LogName"

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Warning "an editor is running; close it first or the run may fight over assets"
}

# test names are "Project.Functional Tests.<map path minus /Game/, dotted>.<actor label>", matched as a substring
$MapSuite = ($Map -replace '^/Game/', '' -replace '^/', '') -replace '/', '.'
$TestFilter = "Project.Functional Tests.$MapSuite"
if ($Filter) { $TestFilter += ".$Filter" }

if (Test-Path $LogPath) { Remove-Item $LogPath -Confirm:$false }

$ArgList = @(
    "`"$Project`"",
    "-ExecCmds=`"Automation RunTests $TestFilter; Quit`"",
    "-unattended", "-nullrhi", "-nosound", "-nosplash", "-nop4",
    "-log=`"$LogName`""
)

Write-Host "running '$TestFilter'"
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgList -PassThru -WindowStyle Hidden
if (-not $Process.WaitForExit($TimeoutSec * 1000)) {
    Stop-Process -Id $Process.Id -Force
    Write-Host "timed out after $TimeoutSec s"
    exit 2
}

if (-not (Test-Path $LogPath)) {
    Write-Host "no log at $LogPath (editor exit code $($Process.ExitCode))"
    exit 2
}

Select-String -Path $LogPath -Pattern 'LogB1Test:' | ForEach-Object { $_.Line -replace '^.*?LogB1Test: (Display: )?', '' }

$Completed = @(Select-String -Path $LogPath -Pattern 'Test Completed\. Result=\{(\w+)\}')
$Failed = @($Completed | Where-Object { $_.Matches[0].Groups[1].Value -ne 'Success' })

Write-Host "tests: $($Completed.Count) ran, $($Failed.Count) failed, editor exit code $($Process.ExitCode)"
Write-Host "log: $LogPath"

if ($Completed.Count -eq 0) {
    Write-Host "no tests matched '$TestFilter'"
    exit 1
}
if ($Failed.Count -gt 0) { exit 1 }
exit 0
