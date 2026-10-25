<#
closes the MudDriver editor the way the window's X button does, so unsaved work triggers the
usual save prompt instead of being lost. exit 1 means it's still open (most likely that prompt).

  .\Scripts\StopEditor.ps1
  .\Scripts\StopEditor.ps1 -Force   # kills the process, unsaved changes are lost
#>
param(
    [int]$TimeoutSec = 90,
    [switch]$Force
)

. (Join-Path $PSScriptRoot "EditorCommon.ps1")

$Editors = @(Get-ProjectEditor)
if ($Editors.Count -eq 0) {
    Write-Host "editor not running"
    exit 0
}

foreach ($Editor in $Editors) {
    $Process = Get-Process -Id $Editor.ProcessId -ErrorAction SilentlyContinue
    if (-not $Process) { continue }

    if ($Force) {
        Stop-Process -Id $Process.Id -Force -Confirm:$false
        Write-Host "killed editor (pid $($Process.Id))"
        continue
    }

    if (-not $Process.CloseMainWindow()) {
        Write-Host "editor (pid $($Process.Id)) has no main window to close; still starting up or hung"
        exit 1
    }
    if (-not $Process.WaitForExit($TimeoutSec * 1000)) {
        Write-Host "editor (pid $($Process.Id)) still open after $TimeoutSec s, probably waiting on a save-changes dialog"
        Write-Host "save via MCP (AssetTools.save_assets) and retry, ask the user, or pass -Force to discard"
        exit 1
    }
    Write-Host "editor closed (pid $($Process.Id))"
}

# live coding console outlives the editor briefly and holds the build mutex
Get-Process LiveCodingConsole -ErrorAction SilentlyContinue | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
exit 0
