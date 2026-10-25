<#
starts the MudDriver editor and waits until the main window is up and the MCP server
(127.0.0.1:8001) accepts connections. build first: stale binaries open a modal rebuild prompt.

  .\Scripts\StartEditor.ps1
  .\Scripts\StartEditor.ps1 -Map /Game/B1/Gym/Lvl_Gym_Mud
#>
param(
    [string]$Map = "",
    [int]$TimeoutSec = 900
)

. (Join-Path $PSScriptRoot "EditorCommon.ps1")

$Running = @(Get-ProjectEditor)
if ($Running.Count -gt 0) {
    Write-Host "editor already running (pid $($Running[0].ProcessId))"
    exit 0
}

$ArgList = @("`"$Project`"")
if ($Map) { $ArgList += $Map }
$ArgList += "-skipcompile"

$Process = Start-Process -FilePath $EditorExe -ArgumentList $ArgList -PassThru
Write-Host "started editor (pid $($Process.Id)), waiting for main window and MCP on port $McpPort"

$Deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $Deadline) {
    Start-Sleep -Seconds 3
    $Process.Refresh()
    if ($Process.HasExited) {
        Write-Host "editor exited during startup (code $($Process.ExitCode)), see $EditorLog"
        exit 1
    }
    # the splash screen is titled "Unreal Editor - MudDriver"; the loaded editor flips it
    if ($Process.MainWindowTitle -like "MudDriver - Unreal Editor*" -and (Test-McpPort)) {
        Write-Host "editor ready: '$($Process.MainWindowTitle)'"
        exit 0
    }
}

Write-Host "editor not ready after $TimeoutSec s (window: '$($Process.MainWindowTitle)'), check for a modal dialog or $EditorLog"
exit 1
