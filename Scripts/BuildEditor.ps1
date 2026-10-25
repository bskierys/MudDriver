<#
full UBT build of MudDriverEditor. closes the editor first if it's open and reopens it after a
successful build. use this for header/UPROPERTY/new-class changes that Live Coding can't do.

  .\Scripts\BuildEditor.ps1
  .\Scripts\BuildEditor.ps1 -NoReopen
#>
param(
    [switch]$NoReopen,
    [switch]$ForceClose,
    [string]$Configuration = "Development"
)

. (Join-Path $PSScriptRoot "EditorCommon.ps1")

$WasRunning = @(Get-ProjectEditor).Count -gt 0
if ($WasRunning) {
    $StopArgs = @{}
    if ($ForceClose) { $StopArgs.Force = $true }
    & (Join-Path $PSScriptRoot "StopEditor.ps1") @StopArgs
    if ($LASTEXITCODE -ne 0) { exit 1 }
}

# UBT doesn't rescan for files the agent's Write tool created, so drop the makefile when there are any
$Makefile = Join-Path $ProjectDir "Intermediate\Build\Win64\x64\MudDriverEditor\$Configuration\Makefile.bin"
if (Test-Path $Makefile) {
    $MakefileTime = (Get-Item $Makefile).LastWriteTime
    $NewFiles = @(Get-ChildItem (Join-Path $ProjectDir "Source") -Recurse -Include *.cpp, *.h |
        Where-Object { $_.CreationTime -gt $MakefileTime })
    if ($NewFiles.Count -gt 0) {
        Write-Host "new source files since last makefile, regenerating: $($NewFiles.Name -join ', ')"
        Remove-Item $Makefile -Confirm:$false
    }
}

$BuildBat = Join-Path $EngineDir "Engine\Build\BatchFiles\Build.bat"
& $BuildBat MudDriverEditor Win64 $Configuration "$Project" -waitmutex
$BuildExit = $LASTEXITCODE
if ($BuildExit -ne 0) {
    Write-Host "build failed (exit $BuildExit); editor left closed"
    exit $BuildExit
}

if ($WasRunning -and -not $NoReopen) {
    & (Join-Path $PSScriptRoot "StartEditor.ps1")
    exit $LASTEXITCODE
}
exit 0
